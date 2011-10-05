/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "Directory.h"
#include "FactoryDirectory.h"
#include "FactoryFileDirectory.h"
#ifndef _LINUX
#include "utils/Win32Exception.h"
#endif
#include "FileItem.h"
#include "DirectoryCache.h"
#include "GUISettings.h"
#include "utils/log.h"
#include "Job.h"
#include "JobManager.h"
#include "Application.h"
#include "GUIWindowManager.h"
#include "GUIDialogBusy.h"
#include "SingleLock.h"
#include "Util.h"

using namespace std;
using namespace XFILE;

#define TIME_TO_BUSY_DIALOG 500

class CGetDirectory
  : IJobCallback
{
private:
  struct CGetJob
    : CJob
  {
    CGetJob(IDirectory& imp
          , const CStdString& dir
          , CFileItemList& list)
      : m_dir(dir)
      , m_list(list)
      , m_imp(imp)      
    {}
  public:
    virtual bool DoWork()
    {
      m_list.m_strPath = m_dir;
      return m_imp.GetDirectory(m_dir, m_list);
    }
    CStdString     m_dir;
    CFileItemList& m_list;
    IDirectory&    m_imp;
  };

public:

  CGetDirectory(IDirectory& imp, const CStdString& dir)
    : m_event(true)
  {
    m_id = CJobManager::GetInstance().AddJob(new CGetJob(imp, dir, m_list)
                                           , this
                                           , CJob::PRIORITY_HIGH);
  }
 ~CGetDirectory()
  {
    CJobManager::GetInstance().CancelJob(m_id);
  }

  virtual void OnJobComplete(unsigned int jobID, bool success, CJob *job)
  {
    m_result = success;
    m_event.Set();
  }

  bool Wait(unsigned int timeout)
  {
    return m_event.WaitMSec(timeout);
  }

  bool GetDirectory(CFileItemList& list)
  {
    m_event.Wait();
    if(!m_result)
    {
      list.Clear();
      return false;
    }
    list.Copy(m_list);
    return true;
  }

  bool          m_result;
  CFileItemList m_list;
  CEvent        m_event;
  unsigned int  m_id;
};





CDirectory::CDirectory()
{}

CDirectory::~CDirectory()
{}

bool CDirectory::GetDirectory(const CStdString& strPath, CFileItemList &items, CStdString strMask /*=""*/, bool bUseFileDirectories /* = true */, bool allowPrompting /* = false */, DIR_CACHE_TYPE cacheDirectory /* = DIR_CACHE_ONCE */, bool extFileInfo /* = true */, bool allowThreads /* = false */, bool getHidden /* = false */)
{
  try
  {
    auto_ptr<IDirectory> pDirectory(CFactoryDirectory::Create(strPath));
    if (!pDirectory.get())
      return false;

    // check our cache for this path
    if (g_directoryCache.GetDirectory(strPath, items, cacheDirectory == DIR_CACHE_ALWAYS))
      items.m_strPath = strPath;
    else
    {
      // need to clear the cache (in case the directory fetch fails)
      // and (re)fetch the folder
      if (cacheDirectory != DIR_CACHE_NEVER)
        g_directoryCache.ClearDirectory(strPath);

      pDirectory->SetAllowPrompting(allowPrompting);
      pDirectory->SetCacheDirectory(cacheDirectory);
      pDirectory->SetUseFileDirectories(bUseFileDirectories);
      pDirectory->SetExtFileInfo(extFileInfo);

      bool result = false;
      while (!result)
      {
        if (g_application.IsCurrentThread() && allowThreads && !CUtil::IsSpecial(strPath))
        {
          CSingleExit ex(g_graphicsContext);

          CGetDirectory get(*pDirectory, strPath);
          if(!get.Wait(TIME_TO_BUSY_DIALOG))
          {
            CGUIDialogBusy* dialog = NULL;
            while(!get.Wait(10))
            {
              CSingleLock lock(g_graphicsContext);
              if(g_windowManager.IsWindowVisible(WINDOW_DIALOG_PROGRESS)
              || g_windowManager.IsWindowVisible(WINDOW_DIALOG_LOCK_SETTINGS))
              {
                if(dialog)
                {
                  dialog->Close();
                  dialog = NULL;
                }
                g_windowManager.Process(false);
              }
              else
              {
                if(dialog == NULL)
                {
                  dialog = (CGUIDialogBusy*)g_windowManager.GetWindow(WINDOW_DIALOG_BUSY);
                  if(dialog)
                    dialog->Show();
                }
                g_windowManager.Process(true);
              }
            }
            if(dialog)
              dialog->Close();
          }
          result = get.GetDirectory(items);
        }
        else
        {
          items.m_strPath = strPath;
          result = pDirectory->GetDirectory(strPath, items);
        }

        if (!result)
        {
          if (g_application.IsCurrentThread() && pDirectory->ProcessRequirements())
            continue;
          CLog::Log(LOGERROR, "%s - Error getting %s", __FUNCTION__, strPath.c_str());
          return false;
        }
      }

      // cache the directory, if necessary
      if (cacheDirectory != DIR_CACHE_NEVER)
        g_directoryCache.SetDirectory(strPath, items, pDirectory->GetCacheType(strPath));
    }

    // now filter for allowed files
    pDirectory->SetMask(strMask);
    for (int i = 0; i < items.Size(); ++i)
    {
      CFileItemPtr item = items[i];
      // TODO: we shouldn't be checking the gui setting here;
      // callers should use getHidden instead
      if ((!item->m_bIsFolder && !pDirectory->IsAllowed(item->m_strPath)) ||
          (item->GetPropertyBOOL("file:hidden") && !getHidden && !g_guiSettings.GetBool("filelists.showhidden")))
      {
        items.Remove(i);
        i--; // don't confuse loop
      }
    }

    //  Should any of the files we read be treated as a directory?
    //  Disable for database folders, as they already contain the extracted items
    if (bUseFileDirectories && !items.IsMusicDb() && !items.IsVideoDb() && !items.IsSmartPlayList())
      FilterFileDirectories(items, strMask);

    return true;
  }
#ifndef _LINUX
  catch (const win32_exception &e)
  {
    e.writelog(__FUNCTION__);
  }
#endif
  catch (...)
  {
    CLog::Log(LOGERROR, "%s - Unhandled exception", __FUNCTION__);
  }
  CLog::Log(LOGERROR, "%s - Error getting %s", __FUNCTION__, strPath.c_str());
  return false;
}

bool CDirectory::Create(const CStdString& strPath)
{
  try
  {
    auto_ptr<IDirectory> pDirectory(CFactoryDirectory::Create(strPath));
    if (pDirectory.get())
      if(pDirectory->Create(strPath.c_str()))
        return true;
  }
#ifndef _LINUX
  catch (const win32_exception &e)
  {
    e.writelog(__FUNCTION__);
  }
#endif
  catch (...)
  {
    CLog::Log(LOGERROR, "%s - Unhandled exception", __FUNCTION__);
  }
  CLog::Log(LOGERROR, "%s - Error creating %s", __FUNCTION__, strPath.c_str());
  return false;
}

bool CDirectory::Exists(const CStdString& strPath)
{
  try
  {
    auto_ptr<IDirectory> pDirectory(CFactoryDirectory::Create(strPath));
    if (pDirectory.get())
      return pDirectory->Exists(strPath.c_str());
  }
#ifndef _LINUX
  catch (const win32_exception &e)
  {
    e.writelog(__FUNCTION__);
  }
#endif
  catch (...)
  {
    CLog::Log(LOGERROR, "%s - Unhandled exception", __FUNCTION__);
  }
  CLog::Log(LOGERROR, "%s - Error checking for %s", __FUNCTION__, strPath.c_str());
  return false;
}

bool CDirectory::Remove(const CStdString& strPath)
{
  try
  {
    auto_ptr<IDirectory> pDirectory(CFactoryDirectory::Create(strPath));
    if (pDirectory.get())
      if(pDirectory->Remove(strPath.c_str()))
        return true;
  }
#ifndef _LINUX
  catch (const win32_exception &e)
  {
    e.writelog(__FUNCTION__);
  }
#endif
  catch (...)
  {
    CLog::Log(LOGERROR, "%s - Unhandled exception", __FUNCTION__);
  }
  CLog::Log(LOGERROR, "%s - Error removing %s", __FUNCTION__, strPath.c_str());
  return false;
}

void CDirectory::FilterFileDirectories(CFileItemList &items, const CStdString &mask)
{
  for (int i=0; i< items.Size(); ++i)
  {
    CFileItemPtr pItem=items[i];
    if ((!pItem->m_bIsFolder) && (!pItem->IsInternetStream()))
    {
      auto_ptr<IFileDirectory> pDirectory(CFactoryFileDirectory::Create(pItem->m_strPath,pItem.get(),mask));
      if (pDirectory.get())
        pItem->m_bIsFolder = true;
      else
        if (pItem->m_bIsFolder)
        {
          items.Remove(i);
          i--; // don't confuse loop
        }
    }
  }
}

/*
 *      Copyright (C) 2011 Team XBMC
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

#include "AddonInstaller.h"
#include "log.h"
#include "Util.h"
#include "LocalizeStrings.h"
#include "FileSystem/Directory.h"
#include "GUISettings.h"
#include "Settings.h"
#include "Application.h"
#include "JobManager.h"
#include "GUIDialogYesNo.h"
#include "addons/AddonManager.h"
#include "addons/Repository.h"
#include "GUIWindowManager.h"      // for callback
#include "GUIUserMessages.h"       // for callback
#include "StringUtils.h"

using namespace std;
using namespace XFILE;
using namespace ADDON;


struct find_map : public binary_function<CAddonInstaller::JobMap::value_type, unsigned int, bool>
{
  bool operator() (CAddonInstaller::JobMap::value_type t, unsigned int id) const
  {
    return (t.second.jobID == id);
  }
};

CAddonInstaller::CAddonInstaller()
{
}

CAddonInstaller::~CAddonInstaller()
{
}

CAddonInstaller &CAddonInstaller::Get()
{
  static CAddonInstaller addonInstaller;
  return addonInstaller;
}

void CAddonInstaller::OnJobComplete(unsigned int jobID, bool success, CJob* job2)
{
  CSingleLock lock(m_critSection);
  JobMap::iterator i = find_if(m_downloadJobs.begin(), m_downloadJobs.end(), bind2nd(find_map(), jobID));
  if (i != m_downloadJobs.end())
    m_downloadJobs.erase(i);
  lock.Leave();

  if (success)
    CAddonMgr::Get().FindAddons();

  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
  g_windowManager.SendThreadMessage(msg);
}

void CAddonInstaller::OnJobProgress(unsigned int jobID, unsigned int progress, unsigned int total, const CJob *job)
{
  CSingleLock lock(m_critSection);
  JobMap::iterator i = find_if(m_downloadJobs.begin(), m_downloadJobs.end(), bind2nd(find_map(), jobID));
  if (i != m_downloadJobs.end())
  { // update job progress
    i->second.progress = progress;
    CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_ITEM);
    msg.SetStringParam(i->first);
    lock.Leave();
    g_windowManager.SendThreadMessage(msg);
  }
}

bool CAddonInstaller::IsDownloading() const
{
  CSingleLock lock(m_critSection);
  return m_downloadJobs.size() > 0;
}

void CAddonInstaller::GetInstallList(VECADDONS &addons) const
{
  CSingleLock lock(m_critSection);
  vector<CStdString> addonIDs;
  for (JobMap::const_iterator i = m_downloadJobs.begin(); i != m_downloadJobs.end(); ++i)
  {
    if (i->second.jobID)
      addonIDs.push_back(i->first);
  }
  lock.Leave();

  CAddonDatabase database;
  database.Open();
  for (vector<CStdString>::iterator it = addonIDs.begin(); it != addonIDs.end();++it)
  {
    AddonPtr addon;
    if (database.GetAddon(*it, addon))
      addons.push_back(addon);
  }
}

bool CAddonInstaller::GetProgress(const CStdString &addonID, unsigned int &percent) const
{
  CSingleLock lock(m_critSection);
  JobMap::const_iterator i = m_downloadJobs.find(addonID);
  if (i != m_downloadJobs.end())
  {
    percent = i->second.progress;
    return true;
  }
  return false;
}

bool CAddonInstaller::Cancel(const CStdString &addonID)
{
  CSingleLock lock(m_critSection);
  JobMap::iterator i = m_downloadJobs.find(addonID);
  if (i != m_downloadJobs.end())
  {
    CJobManager::GetInstance().CancelJob(i->second.jobID);
    m_downloadJobs.erase(i);
    return true;
  }
  return false;
}

void CAddonInstaller::Install(const CStdString &addonID, bool force, const CStdString &referer, bool background)
{
  AddonPtr addon;
  bool addonInstalled = CAddonMgr::Get().GetAddon(addonID, addon);
  if (addonInstalled && !force)
    return;

  // check whether we have it available in a repository
  CAddonDatabase database;
  database.Open();
  if (database.GetAddon(addonID, addon))
  {
    CStdString repo;
    database.GetRepoForAddon(addonID,repo);
    AddonPtr ptr;
    CAddonMgr::Get().GetAddon(repo,ptr);
    RepositoryPtr therepo = boost::dynamic_pointer_cast<CRepository>(ptr);
    CStdString hash;
    if (therepo)
      hash = therepo->GetAddonHash(addon);

    // check whether we already have the addon installing
    CSingleLock lock(m_critSection);
    if (m_downloadJobs.find(addonID) != m_downloadJobs.end())
      return; // already installing this addon

    if (background)
    {
      unsigned int jobID = CJobManager::GetInstance().AddJob(new CAddonInstallJob(addon, hash, addonInstalled, referer), this);
      m_downloadJobs.insert(make_pair(addon->ID(), CDownloadJob(jobID)));
    }
    else
    {
      m_downloadJobs.insert(make_pair(addon->ID(), CDownloadJob(0)));
      lock.Leave();
      CAddonInstallJob job(addon, hash, addonInstalled, referer);
      if (!job.DoWork())
      { // TODO: dump something to debug log?
      }
      lock.Enter();
      JobMap::iterator i = m_downloadJobs.find(addon->ID());
      m_downloadJobs.erase(i);
    }
  }
}

bool CAddonInstaller::InstallFromZip(const CStdString &path)
{
  // grab the descriptive XML document from the zip, and read it in
  CFileItemList items;
  // BUG: some zip files return a single item (root folder) that we think is stored, so we don't use the zip:// protocol
  CStdString zipDir;
  CUtil::CreateArchivePath(zipDir, "zip", path, "");
  if (!CDirectory::GetDirectory(zipDir, items) || items.Size() != 1 || !items[0]->m_bIsFolder)
    return false;

  // TODO: possibly add support for github generated zips here?
  CStdString archive = CUtil::AddFileToFolder(items[0]->m_strPath, "addon.xml");

  TiXmlDocument xml;
  AddonPtr addon;
  if (xml.LoadFile(archive) && CAddonMgr::Get().LoadAddonDescriptionFromMemory(xml.RootElement(), addon))
  {
    // set the correct path
    addon->Props().path = path;

    // install the addon
    CSingleLock lock(m_critSection);
    if (m_downloadJobs.find(addon->ID()) != m_downloadJobs.end())
      return false;

    unsigned int jobID = CJobManager::GetInstance().AddJob(new CAddonInstallJob(addon), this);
    m_downloadJobs.insert(make_pair(addon->ID(), CDownloadJob(jobID)));
    return true;
  }
  return false;
}

void CAddonInstaller::InstallFromXBMCRepo(const set<CStdString> &addonIDs)
{
  // first check we have the main repository updated...
  AddonPtr addon;
  if (CAddonMgr::Get().GetAddon("repository.xbmc.org", addon))
  {
    VECADDONS addons;
    CAddonDatabase database;
    database.Open();
    if (!database.GetRepository(addon->ID(), addons))
    {
      RepositoryPtr repo = boost::dynamic_pointer_cast<CRepository>(addon);
      addons = CRepositoryUpdateJob::GrabAddons(repo, false);
    }
  }
  // now install the addons
  for (set<CStdString>::const_iterator i = addonIDs.begin(); i != addonIDs.end(); ++i)
    CAddonInstaller::Get().Install(*i);
}

CAddonInstallJob::CAddonInstallJob(const AddonPtr &addon, const CStdString &hash, bool update, const CStdString &referer)
: m_addon(addon), m_hash(hash), m_update(update), m_referer(referer)
{
}

bool CAddonInstallJob::DoWork()
{
  // Addons are installed by downloading the .zip package on the server to the local
  // packages folder, then extracting from the local .zip package into the addons folder
  // Both these functions are achieved by "copying" using the vfs.

  CStdString dest="special://home/addons/packages/";
  CStdString package = CUtil::AddFileToFolder("special://home/addons/packages/",
                                              CUtil::GetFileName(m_addon->Path()));

  CStdString installFrom;
  if (CUtil::HasSlashAtEnd(m_addon->Path()))
  { // passed in a folder - all we need do is copy it across
    installFrom = m_addon->Path();
  }
  else
  {
    // zip passed in - download + extract
    CStdString path(m_addon->Path());
    if (!m_referer.IsEmpty() && CUtil::IsInternetStream(path))
    {
      CURL url(path);
      url.SetProtocolOptions(m_referer);
      path = url.Get();
    }
    if (!CFile::Exists(package) && !DownloadPackage(path, dest))
    {
      CFile::Delete(package);
      return false;
    }

    // at this point we have the package - check that it is valid
    if (!CFile::Exists(package) || !CheckHash(package))
    {
      CFile::Delete(package);
      return false;
    }

    // check the archive as well - should have just a single folder in the root
    CStdString archive;
    CUtil::CreateArchivePath(archive,"zip",package,"");

    CFileItemList archivedFiles;
    CDirectory::GetDirectory(archive, archivedFiles);

    if (archivedFiles.Size() != 1 || !archivedFiles[0]->m_bIsFolder)
    { // invalid package
      CFile::Delete(package);
      return false;
    }
    installFrom = archivedFiles[0]->m_strPath;
  }

  // run any pre-install functions
  bool reloadAddon = OnPreInstall();

  // perform install
  if (!Install(installFrom))
    return false; // something went wrong

  // run any post-install guff
  OnPostInstall(reloadAddon);

  // and we're done!
  return true;
}

bool CAddonInstallJob::DownloadPackage(const CStdString &path, const CStdString &dest)
{ // need to download/copy the package first
  CFileItemList list;
  list.Add(CFileItemPtr(new CFileItem(path,false)));
  list[0]->Select(true);
  SetFileOperation(CFileOperationJob::ActionReplace, list, dest);
  return CFileOperationJob::DoWork();
}

bool CAddonInstallJob::OnPreInstall()
{
  // check whether this is an active skin - we need to unload it if so
  if (g_guiSettings.GetString("lookandfeel.skin") == m_addon->ID())
  {
    g_application.getApplicationMessenger().ExecBuiltIn("UnloadSkin", true);
    return true;
  }
  return false;
}

bool CAddonInstallJob::Install(const CStdString &installFrom)
{
  CStdString addonFolder(installFrom);
  CUtil::RemoveSlashAtEnd(addonFolder);
  addonFolder = CUtil::AddFileToFolder("special://home/addons/",
                                       CUtil::GetFileName(addonFolder));

  CFileItemList install;
  install.Add(CFileItemPtr(new CFileItem(installFrom, true)));
  install[0]->Select(true);
  CFileOperationJob job(CFileOperationJob::ActionReplace, install, "special://home/addons/");

  AddonPtr addon;
  if (!job.DoWork() || !CAddonMgr::Get().LoadAddonDescription(addonFolder, addon))
  { // failed extraction or failed to load addon description
    CStdString addonID = CUtil::GetFileName(addonFolder);
    ReportInstallError(addonID, addonID);
    CLog::Log(LOGERROR,"Could not read addon description of %s", addonID.c_str());
    CFileItemList list;
    list.Add(CFileItemPtr(new CFileItem(addonFolder, true)));
    list[0]->Select(true);
    CJobManager::GetInstance().AddJob(new CFileOperationJob(CFileOperationJob::ActionDelete, list, ""), NULL);
    return false;
  }

  // resolve dependencies
  CAddonMgr::Get().FindAddons(); // needed as GetDeps() grabs directly from c-pluff via the addon manager
  ADDONDEPS deps = addon->GetDeps();
  CStdString referer;
  referer.Format("Referer=%s-%s.zip",addon->ID().c_str(),addon->Version().str.c_str());
  for (ADDONDEPS::iterator it  = deps.begin(); it != deps.end(); ++it)
  {
    if (it->first.Equals("xbmc.metadata"))
      continue;
    AddonPtr dependency;
    if (!CAddonMgr::Get().GetAddon(it->first,dependency))
      CAddonInstaller::Get().Install(it->first, false, referer, false); // no new job for these
  }
  return true;
}

void CAddonInstallJob::OnPostInstall(bool reloadAddon)
{
  if (m_addon->Type() < ADDON_VIZ_LIBRARY && g_settings.m_bAddonNotifications)
  {
    g_application.m_guiDialogKaiToast.QueueNotification(m_addon->Icon(),
                                                        m_addon->Name(),
                                                        g_localizeStrings.Get(m_update ? 24065 : 24064),
                                                        TOAST_DISPLAY_TIME,false,
                                                        TOAST_DISPLAY_TIME);
  }
  if (m_addon->Type() == ADDON_SKIN)
  {
    if (reloadAddon || CGUIDialogYesNo::ShowAndGetInput(m_addon->Name(),
                                                        g_localizeStrings.Get(24099),"",""))
    {
      g_guiSettings.SetString("lookandfeel.skin",m_addon->ID().c_str());
      g_application.m_guiDialogKaiToast.ResetTimer();
      g_application.m_guiDialogKaiToast.Close(true);
      g_application.getApplicationMessenger().ExecBuiltIn("ReloadSkin");
    }
  }
}

void CAddonInstallJob::ReportInstallError(const CStdString& addonID,
                                                const CStdString& fileName)
{
  AddonPtr addon;
  CAddonDatabase database;
  database.Open();
  database.GetAddon(addonID, addon);
  if (addon)
  {
    AddonPtr addon2;
    CAddonMgr::Get().GetAddon(addonID, addon2);
    g_application.m_guiDialogKaiToast.QueueNotification(
                                                        addon->Icon(),
                                                        addon->Name(),
                                                        g_localizeStrings.Get(addon2 ? 113 : 114),
                                                        TOAST_DISPLAY_TIME, false);
  }
  else
  {
    g_application.m_guiDialogKaiToast.QueueNotification(
                                                        CGUIDialogKaiToast::Error,
                                                        fileName,
                                                        g_localizeStrings.Get(114),
                                                        TOAST_DISPLAY_TIME, false);
  }
}

bool CAddonInstallJob::CheckHash(const CStdString& zipFile)
{
  if (m_hash.IsEmpty())
    return true;
  CStdString md5 = CUtil::GetFileMD5(zipFile);
  if (!md5.Equals(m_hash))
  {
    CFile::Delete(zipFile);
    ReportInstallError(m_addon->ID(), CUtil::GetFileName(zipFile));
    CLog::Log(LOGERROR, "MD5 mismatch after download %s", zipFile.c_str());
    return false;
  }
  return true;
}

CStdString CAddonInstallJob::AddonID() const
{
  return (m_addon) ? m_addon->ID() : "";
}

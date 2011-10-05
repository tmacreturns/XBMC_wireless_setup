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


// FileShoutcast.cpp: implementation of the CFileShoutcast class.
//
//////////////////////////////////////////////////////////////////////

#include "system.h"
#include "AdvancedSettings.h"
#include "Application.h"
#include "FileShoutcast.h"
#include "GUISettings.h"
#include "GUIDialogProgress.h"
#include "GUIWindowManager.h"
#include "URL.h"
#include "utils/TimeUtils.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#include "utils/GUIInfoManager.h"
#include "utils/log.h"

using namespace XFILE;
using namespace MUSIC_INFO;

CFileShoutcast::CFileShoutcast()
{
  m_lastTime = CTimeUtils::GetTimeMS();
  m_discarded = 0;
  m_currint = 0;
  m_buffer = NULL;
}

CFileShoutcast::~CFileShoutcast()
{
  Close();
}

int64_t CFileShoutcast::GetPosition()
{
  return m_file.GetPosition()-m_discarded;
}

int64_t CFileShoutcast::GetLength()
{
  return 0;
}

bool CFileShoutcast::Open(const CURL& url)
{
  CURL url2(url);
  url2.SetProtocolOptions("noshout=true&Icy-MetaData=1");
  url2.SetProtocol("http");

  bool result=false;
  if ((result=m_file.Open(url2.Get())))
  {
    m_tag.SetTitle(m_file.GetHttpHeader().GetValue("icy-name"));
    if (m_tag.GetTitle().IsEmpty())
      m_tag.SetTitle(m_file.GetHttpHeader().GetValue("ice-name")); // icecast
    m_tag.SetGenre(m_file.GetHttpHeader().GetValue("icy-genre"));
    if (m_tag.GetGenre().IsEmpty())
      m_tag.SetGenre(m_file.GetHttpHeader().GetValue("ice-genre")); // icecast
    m_tag.SetLoaded(true);
    g_infoManager.SetCurrentSongTag(m_tag);
  }
  m_metaint = atoi(m_file.GetHttpHeader().GetValue("icy-metaint").c_str());
  m_buffer = new char[16*255];

  return result;
}

unsigned int CFileShoutcast::Read(void* lpBuf, int64_t uiBufSize)
{
  if (m_currint >= m_metaint && m_metaint > 0)
  {
    unsigned char header;
    m_file.Read(&header,1);
    ReadTruncated(m_buffer, header*16);
    ExtractTagInfo(m_buffer);
    m_discarded += header*16+1;
    m_currint = 0;
  }
  if (CTimeUtils::GetTimeMS() - m_lastTime > 500)
  {
    m_lastTime = CTimeUtils::GetTimeMS();
    g_infoManager.SetCurrentSongTag(m_tag);
  }

  unsigned int toRead = std::min((unsigned int)uiBufSize,(unsigned int)m_metaint-m_currint);
  toRead = m_file.Read(lpBuf,toRead);
  m_currint += toRead;
  return toRead;
}

int64_t CFileShoutcast::Seek(int64_t iFilePosition, int iWhence)
{
  return -1;
}

void CFileShoutcast::Close()
{
  delete[] m_buffer;
  m_file.Close();
}

void CFileShoutcast::ExtractTagInfo(const char* buf)
{
  char temp[1024];
  if (sscanf(buf,"StreamTitle='%[^']",temp) > 0)
    m_tag.SetTitle(temp);
}

void CFileShoutcast::ReadTruncated(char* buf2, int size)
{
  char* buf = buf2;
  while (size > 0)
  {
    int read = m_file.Read(buf,size);
    size -= read;
    buf += read;
  }
}


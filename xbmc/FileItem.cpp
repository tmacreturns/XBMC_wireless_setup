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

#include "FileItem.h"
#include "LocalizeStrings.h"
#include "StringUtils.h"
#include "Util.h"
#include "Picture.h"
#include "PlayListFactory.h"
#include "Shortcut.h"
#include "Crc32.h"
#include "FileSystem/DirectoryCache.h"
#include "FileSystem/StackDirectory.h"
#include "FileSystem/FileCurl.h"
#include "FileSystem/MultiPathDirectory.h"
#include "FileSystem/MusicDatabaseDirectory.h"
#include "FileSystem/VideoDatabaseDirectory.h"
#include "FileSystem/FactoryDirectory.h"
#include "MusicInfoTagLoaderFactory.h"
#include "CueDocument.h"
#include "VideoDatabase.h"
#include "MusicDatabase.h"
#include "SortFileItem.h"
#include "utils/TuxBoxUtil.h"
#include "VideoInfoTag.h"
#include "utils/SingleLock.h"
#include "MusicInfoTag.h"
#include "PictureInfoTag.h"
#include "Artist.h"
#include "Album.h"
#include "Song.h"
#include "URL.h"
#include "GUISettings.h"
#include "AdvancedSettings.h"
#include "Settings.h"
#include "utils/RegExp.h"
#include "utils/log.h"
#include "karaoke/karaokelyricsfactory.h"

using namespace std;
using namespace XFILE;
using namespace PLAYLIST;
using namespace MUSIC_INFO;

CFileItem::CFileItem(const CSong& song)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(song.strTitle);
  m_strPath = song.strFileName;
  GetMusicInfoTag()->SetSong(song);
  m_lStartOffset = song.iStartOffset;
  m_lEndOffset = song.iEndOffset;
  m_strThumbnailImage = song.strThumb;
}

CFileItem::CFileItem(const CStdString &path, const CAlbum& album)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(album.strAlbum);
  m_strPath = path;
  m_bIsFolder = true;
  m_strLabel2 = album.strArtist;
  CUtil::AddSlashAtEnd(m_strPath);
  GetMusicInfoTag()->SetAlbum(album);
  if (album.thumbURL.m_url.size() > 0)
    m_strThumbnailImage = album.thumbURL.m_url[0].m_url;
  else
    m_strThumbnailImage.clear();
  m_bIsAlbum = true;
  CMusicDatabase::SetPropertiesFromAlbum(*this,album);
}

CFileItem::CFileItem(const CVideoInfoTag& movie)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(movie.m_strTitle);
  if (movie.m_strFileNameAndPath.IsEmpty())
  {
    m_strPath = movie.m_strPath;
    CUtil::AddSlashAtEnd(m_strPath);
    m_bIsFolder = true;
  }
  else
  {
    m_strPath = movie.m_strFileNameAndPath;
    m_bIsFolder = false;
  }
  *GetVideoInfoTag() = movie;
  FillInDefaultIcon();
  SetCachedVideoThumb();
}

CFileItem::CFileItem(const CArtist& artist)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(artist.strArtist);
  m_strPath = artist.strArtist;
  m_bIsFolder = true;
  CUtil::AddSlashAtEnd(m_strPath);
  GetMusicInfoTag()->SetArtist(artist.strArtist);
}

CFileItem::CFileItem(const CGenre& genre)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(genre.strGenre);
  m_strPath = genre.strGenre;
  m_bIsFolder = true;
  CUtil::AddSlashAtEnd(m_strPath);
  GetMusicInfoTag()->SetGenre(genre.strGenre);
}

CFileItem::CFileItem(const CFileItem& item): CGUIListItem()
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  *this = item;
}

CFileItem::CFileItem(const CGUIListItem& item)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  // not particularly pretty, but it gets around the issue of Reset() defaulting
  // parameters in the CGUIListItem base class.
  *((CGUIListItem *)this) = item;
}

CFileItem::CFileItem(void)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
}

CFileItem::CFileItem(const CStdString& strLabel)
    : CGUIListItem()
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  SetLabel(strLabel);
}

CFileItem::CFileItem(const CStdString& strPath, bool bIsFolder)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  m_strPath = strPath;
  m_bIsFolder = bIsFolder;
  // tuxbox urls cannot have a / at end
  if (m_bIsFolder && !m_strPath.IsEmpty() && !IsFileFolder() && !CUtil::IsTuxBox(m_strPath))
    CUtil::AddSlashAtEnd(m_strPath);
}

CFileItem::CFileItem(const CMediaSource& share)
{
  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
  Reset();
  m_bIsFolder = true;
  m_bIsShareOrDrive = true;
  m_strPath = share.strPath;
  CUtil::AddSlashAtEnd(m_strPath);
  CStdString label = share.strName;
  if (!share.strStatus.IsEmpty())
    label.Format("%s (%s)", share.strName.c_str(), share.strStatus.c_str());
  SetLabel(label);
  m_iLockMode = share.m_iLockMode;
  m_strLockCode = share.m_strLockCode;
  m_iHasLock = share.m_iHasLock;
  m_iBadPwdCount = share.m_iBadPwdCount;
  m_iDriveType = share.m_iDriveType;
  m_strThumbnailImage = share.m_strThumbnailImage;
  SetLabelPreformated(true);
}

CFileItem::~CFileItem(void)
{
  delete m_musicInfoTag;
  delete m_videoInfoTag;
  delete m_pictureInfoTag;

  m_musicInfoTag = NULL;
  m_videoInfoTag = NULL;
  m_pictureInfoTag = NULL;
}

const CFileItem& CFileItem::operator=(const CFileItem& item)
{
  if (this == &item) return * this;
  CGUIListItem::operator=(item);
  m_bLabelPreformated=item.m_bLabelPreformated;
  FreeMemory();
  m_strPath = item.m_strPath;
  m_bIsParentFolder = item.m_bIsParentFolder;
  m_iDriveType = item.m_iDriveType;
  m_bIsShareOrDrive = item.m_bIsShareOrDrive;
  m_dateTime = item.m_dateTime;
  m_dwSize = item.m_dwSize;
  if (item.HasMusicInfoTag())
  {
    m_musicInfoTag = GetMusicInfoTag();
    if (m_musicInfoTag)
      *m_musicInfoTag = *item.m_musicInfoTag;
  }
  else
  {
    delete m_musicInfoTag;
    m_musicInfoTag = NULL;
  }

  if (item.HasVideoInfoTag())
  {
    m_videoInfoTag = GetVideoInfoTag();
    if (m_videoInfoTag)
      *m_videoInfoTag = *item.m_videoInfoTag;
  }
  else
  {
    delete m_videoInfoTag;
    m_videoInfoTag = NULL;
  }

  if (item.HasPictureInfoTag())
  {
    m_pictureInfoTag = GetPictureInfoTag();
    if (m_pictureInfoTag)
      *m_pictureInfoTag = *item.m_pictureInfoTag;
  }
  else
  {
    delete m_pictureInfoTag;
    m_pictureInfoTag = NULL;
  }

  m_lStartOffset = item.m_lStartOffset;
  m_lEndOffset = item.m_lEndOffset;
  m_strDVDLabel = item.m_strDVDLabel;
  m_strTitle = item.m_strTitle;
  m_iprogramCount = item.m_iprogramCount;
  m_idepth = item.m_idepth;
  m_iLockMode = item.m_iLockMode;
  m_strLockCode = item.m_strLockCode;
  m_iHasLock = item.m_iHasLock;
  m_iBadPwdCount = item.m_iBadPwdCount;
  m_bCanQueue=item.m_bCanQueue;
  m_mimetype = item.m_mimetype;
  m_extrainfo = item.m_extrainfo;
  m_specialSort = item.m_specialSort;
  return *this;
}

void CFileItem::Reset()
{
  m_strLabel2.Empty();
  SetLabel("");
  m_bLabelPreformated=false;
  FreeIcons();
  m_overlayIcon = ICON_OVERLAY_NONE;
  m_bSelected = false;
  m_bIsAlbum = false;
  m_strDVDLabel.Empty();
  m_strTitle.Empty();
  m_strPath.Empty();
  m_dwSize = 0;
  m_bIsFolder = false;
  m_bIsParentFolder=false;
  m_bIsShareOrDrive = false;
  m_dateTime.Reset();
  m_iDriveType = CMediaSource::SOURCE_TYPE_UNKNOWN;
  m_lStartOffset = 0;
  m_lEndOffset = 0;
  m_iprogramCount = 0;
  m_idepth = 1;
  m_iLockMode = LOCK_MODE_EVERYONE;
  m_strLockCode = "";
  m_iBadPwdCount = 0;
  m_iHasLock = 0;
  m_bCanQueue=true;
  m_mimetype = "";
  delete m_musicInfoTag;
  m_musicInfoTag=NULL;
  delete m_videoInfoTag;
  m_videoInfoTag=NULL;
  delete m_pictureInfoTag;
  m_pictureInfoTag=NULL;
  m_extrainfo.Empty();
  m_specialSort = SORT_NORMALLY;
  SetInvalid();
}

void CFileItem::Serialize(CArchive& ar)
{
  CGUIListItem::Serialize(ar);

  if (ar.IsStoring())
  {
    ar << m_bIsParentFolder;
    ar << m_bLabelPreformated;
    ar << m_strPath;
    ar << m_bIsShareOrDrive;
    ar << m_iDriveType;
    ar << m_dateTime;
    ar << m_dwSize;
    ar << m_strDVDLabel;
    ar << m_strTitle;
    ar << m_iprogramCount;
    ar << m_idepth;
    ar << m_lStartOffset;
    ar << m_lEndOffset;
    ar << m_iLockMode;
    ar << m_strLockCode;
    ar << m_iBadPwdCount;

    ar << m_bCanQueue;
    ar << m_mimetype;
    ar << m_extrainfo;
    ar << m_specialSort;

    if (m_musicInfoTag)
    {
      ar << 1;
      ar << *m_musicInfoTag;
    }
    else
      ar << 0;
    if (m_videoInfoTag)
    {
      ar << 1;
      ar << *m_videoInfoTag;
    }
    else
      ar << 0;
    if (m_pictureInfoTag)
    {
      ar << 1;
      ar << *m_pictureInfoTag;
    }
    else
      ar << 0;
  }
  else
  {
    ar >> m_bIsParentFolder;
    ar >> m_bLabelPreformated;
    ar >> m_strPath;
    ar >> m_bIsShareOrDrive;
    ar >> m_iDriveType;
    ar >> m_dateTime;
    ar >> m_dwSize;
    ar >> m_strDVDLabel;
    ar >> m_strTitle;
    ar >> m_iprogramCount;
    ar >> m_idepth;
    ar >> m_lStartOffset;
    ar >> m_lEndOffset;
    int temp;
    ar >> temp;
    m_iLockMode = (LockType)temp;
    ar >> m_strLockCode;
    ar >> m_iBadPwdCount;

    ar >> m_bCanQueue;
    ar >> m_mimetype;
    ar >> m_extrainfo;
    ar >> temp;
    m_specialSort = (SPECIAL_SORT)temp;

    int iType;
    ar >> iType;
    if (iType == 1)
      ar >> *GetMusicInfoTag();
    ar >> iType;
    if (iType == 1)
      ar >> *GetVideoInfoTag();
    ar >> iType;
    if (iType == 1)
      ar >> *GetPictureInfoTag();

    SetInvalid();
  }
}
bool CFileItem::Exists(bool bUseCache /* = true */) const
{
  if (m_strPath.IsEmpty()
   || m_strPath.Equals("add")
   || IsInternetStream()
   || IsParentFolder()
   || IsVirtualDirectoryRoot()
   || IsPlugin())
    return true;

  if (IsVideoDb() && HasVideoInfoTag())
  {
    CFileItem dbItem(m_bIsFolder ? GetVideoInfoTag()->m_strPath : GetVideoInfoTag()->m_strFileNameAndPath, m_bIsFolder);
    return dbItem.Exists();
  }

  CStdString strPath = m_strPath;

  if (CUtil::IsMultiPath(strPath))
    strPath = CMultiPathDirectory::GetFirstPath(strPath);

  if (CUtil::IsStack(strPath))
    strPath = CStackDirectory::GetFirstStackedFile(strPath);

  if (m_bIsFolder)
    return CDirectory::Exists(strPath);
  else
    return CFile::Exists(strPath, bUseCache);

  return false;
}

bool CFileItem::IsVideo() const
{
  /* check preset mime type */
  if( m_mimetype.Left(6).Equals("video/") )
    return true;

  if (HasVideoInfoTag()) return true;
  if (HasMusicInfoTag()) return false;
  if (HasPictureInfoTag()) return false;

  if (IsHDHomeRun() || IsTuxBox() || CUtil::IsDVD(m_strPath))
    return true;

  CStdString extension;
  if( m_mimetype.Left(12).Equals("application/") )
  { /* check for some standard types */
    extension = m_mimetype.Mid(12);
    if( extension.Equals("ogg")
     || extension.Equals("mp4")
     || extension.Equals("mxf") )
     return true;
  }

  CUtil::GetExtension(m_strPath, extension);

  if (extension.IsEmpty())
    return false;

  extension.ToLower();

  if (g_settings.m_videoExtensions.Find(extension) != -1)
    return true;

  return false;
}

bool CFileItem::IsAudio() const
{
  /* check preset mime type */
  if( m_mimetype.Left(6).Equals("audio/") )
    return true;

  if (HasMusicInfoTag()) return true;
  if (HasVideoInfoTag()) return false;
  if (HasPictureInfoTag()) return false;
  if (IsCDDA()) return true;
  if (!m_bIsFolder && IsLastFM()) return true;

  CStdString extension;
  if( m_mimetype.Left(12).Equals("application/") )
  { /* check for some standard types */
    extension = m_mimetype.Mid(12);
    if( extension.Equals("ogg")
     || extension.Equals("mp4")
     || extension.Equals("mxf") )
     return true;
  }

  CUtil::GetExtension(m_strPath, extension);

  if (extension.IsEmpty())
    return false;

  extension.ToLower();
  if (g_settings.m_musicExtensions.Find(extension) != -1)
    return true;

  return false;
}

bool CFileItem::IsKaraoke() const
{
  if ( !IsAudio() || IsLastFM())
    return false;

  return CKaraokeLyricsFactory::HasLyrics( m_strPath );
}

bool CFileItem::IsPicture() const
{
  if( m_mimetype.Left(6).Equals("image/") )
    return true;

  if (HasPictureInfoTag()) return true;
  if (HasMusicInfoTag()) return false;
  if (HasVideoInfoTag()) return false;

  return CUtil::IsPicture(m_strPath);
}

bool CFileItem::IsLyrics() const
{
  return CUtil::GetExtension(m_strPath).Equals(".cdg", false) || CUtil::GetExtension(m_strPath).Equals(".lrc", false);
}

bool CFileItem::IsCUESheet() const
{
  return CUtil::GetExtension(m_strPath).Equals(".cue", false);
}

bool CFileItem::IsLastFM() const
{
  return CUtil::IsLastFM(m_strPath);
}

bool CFileItem::IsInternetStream() const
{
  if (HasProperty("IsHTTPDirectory"))
    return false;

  return CUtil::IsInternetStream(m_strPath);
}

bool CFileItem::IsFileFolder() const
{
  return (
    IsSmartPlayList() ||
   (IsPlayList() && g_advancedSettings.m_playlistAsFolders) ||
    IsZIP() ||
    IsRAR() ||
    IsRSS() ||
    IsType(".ogg") ||
    IsType(".nsf") ||
    IsType(".sid") ||
    IsType(".sap")
    );
}


bool CFileItem::IsSmartPlayList() const
{
  CStdString strExtension;
  CUtil::GetExtension(m_strPath, strExtension);
  strExtension.ToLower();
  return (strExtension == ".xsp");
}

bool CFileItem::IsPlayList() const
{
  return CPlayListFactory::IsPlaylist(*this);
}

bool CFileItem::IsPythonScript() const
{
  return CUtil::GetExtension(m_strPath).Equals(".py", false);
}

bool CFileItem::IsXBE() const
{
  return CUtil::GetExtension(m_strPath).Equals(".xbe", false);
}

bool CFileItem::IsType(const char *ext) const
{
  return CUtil::GetExtension(m_strPath).Equals(ext, false);
}

bool CFileItem::IsShortCut() const
{
  return CUtil::GetExtension(m_strPath).Equals(".cut", false);
}

bool CFileItem::IsNFO() const
{
  return CUtil::GetExtension(m_strPath).Equals(".nfo", false);
}

bool CFileItem::IsDVDImage() const
{
  CStdString strExtension;
  CUtil::GetExtension(m_strPath, strExtension);
  if (strExtension.Equals(".img") || strExtension.Equals(".iso") || strExtension.Equals(".nrg")) return true;
  return false;
}

bool CFileItem::IsDVDFile(bool bVobs /*= true*/, bool bIfos /*= true*/) const
{
  CStdString strFileName = CUtil::GetFileName(m_strPath);
  if (bIfos)
  {
    if (strFileName.Equals("video_ts.ifo")) return true;
    if (strFileName.Left(4).Equals("vts_") && strFileName.Right(6).Equals("_0.ifo") && strFileName.length() == 12) return true;
  }
  if (bVobs)
  {
    if (strFileName.Equals("video_ts.vob")) return true;
    if (strFileName.Left(4).Equals("vts_") && strFileName.Right(4).Equals(".vob")) return true;
  }

  return false;
}

bool CFileItem::IsRAR() const
{
  return CUtil::IsRAR(m_strPath);
}

bool CFileItem::IsZIP() const
{
  return CUtil::IsZIP(m_strPath);
}

bool CFileItem::IsCBZ() const
{
  return CUtil::GetExtension(m_strPath).Equals(".cbz", false);
}

bool CFileItem::IsCBR() const
{
  return CUtil::GetExtension(m_strPath).Equals(".cbr", false);
}

bool CFileItem::IsRSS() const
{
  if (m_strPath.Left(6).Equals("rss://"))
    return true;

  return CUtil::GetExtension(m_strPath).Equals(".rss")
      || GetMimeType() == "application/rss+xml";
}

bool CFileItem::IsStack() const
{
  return CUtil::IsStack(m_strPath);
}

bool CFileItem::IsPlugin() const
{
  return CUtil::IsPlugin(m_strPath);
}

bool CFileItem::IsAddonsPath() const
{
  return CUtil::IsAddonsPath(m_strPath);
}

bool CFileItem::IsMultiPath() const
{
  return CUtil::IsMultiPath(m_strPath);
}

bool CFileItem::IsCDDA() const
{
  return CUtil::IsCDDA(m_strPath);
}

bool CFileItem::IsDVD() const
{
  return CUtil::IsDVD(m_strPath) || m_iDriveType == CMediaSource::SOURCE_TYPE_DVD;
}

bool CFileItem::IsOnDVD() const
{
  return CUtil::IsOnDVD(m_strPath) || m_iDriveType == CMediaSource::SOURCE_TYPE_DVD;
}

bool CFileItem::IsOnLAN() const
{
  return CUtil::IsOnLAN(m_strPath);
}

bool CFileItem::IsISO9660() const
{
  return CUtil::IsISO9660(m_strPath);
}

bool CFileItem::IsRemote() const
{
  return CUtil::IsRemote(m_strPath);
}

bool CFileItem::IsSmb() const
{
  return CUtil::IsSmb(m_strPath);
}

bool CFileItem::IsXBMS() const
{
  return CUtil::IsXBMS(m_strPath);
}

bool CFileItem::IsURL() const
{
  return CUtil::IsURL(m_strPath);
}

bool CFileItem::IsDAAP() const
{
  return CUtil::IsDAAP(m_strPath);
}

bool CFileItem::IsTuxBox() const
{
  return CUtil::IsTuxBox(m_strPath);
}

bool CFileItem::IsMythTV() const
{
  return CUtil::IsMythTV(m_strPath);
}

bool CFileItem::IsHDHomeRun() const
{
  return CUtil::IsHDHomeRun(m_strPath);
}

bool CFileItem::IsVTP() const
{
  return CUtil::IsVTP(m_strPath);
}

bool CFileItem::IsLiveTV() const
{
  return CUtil::IsLiveTV(m_strPath);
}

bool CFileItem::IsHD() const
{
  return CUtil::IsHD(m_strPath);
}

bool CFileItem::IsMusicDb() const
{
  CURL url(m_strPath);
  return url.GetProtocol().Equals("musicdb");
}

bool CFileItem::IsVideoDb() const
{
  CURL url(m_strPath);
  return url.GetProtocol().Equals("videodb");
}

bool CFileItem::IsVirtualDirectoryRoot() const
{
  return (m_bIsFolder && m_strPath.IsEmpty());
}

bool CFileItem::IsRemovable() const
{
  return IsOnDVD() || IsCDDA() || m_iDriveType == CMediaSource::SOURCE_TYPE_REMOVABLE;
}

bool CFileItem::IsReadOnly() const
{
  if (IsParentFolder()) return true;
  if (m_bIsShareOrDrive) return true;
  return !CUtil::SupportsFileOperations(m_strPath);
}

void CFileItem::FillInDefaultIcon()
{
  //CLog::Log(LOGINFO, "FillInDefaultIcon(%s)", pItem->GetLabel().c_str());
  // find the default icon for a file or folder item
  // for files this can be the (depending on the file type)
  //   default picture for photo's
  //   default picture for songs
  //   default picture for videos
  //   default picture for shortcuts
  //   default picture for playlists
  //   or the icon embedded in an .xbe
  //
  // for folders
  //   for .. folders the default picture for parent folder
  //   for other folders the defaultFolder.png

  if (GetIconImage().IsEmpty())
  {
    if (!m_bIsFolder)
    {
      /* To reduce the average runtime of this code, this list should
       * be ordered with most frequently seen types first.  Also bear
       * in mind the complexity of the code behind the check in the
       * case of IsWhatater() returns false.
       */
      if ( IsAudio() )
      {
        // audio
        SetIconImage("DefaultAudio.png");
      }
      else if ( IsVideo() )
      {
        // video
        SetIconImage("DefaultVideo.png");
      }
      else if ( IsPicture() )
      {
        // picture
        SetIconImage("DefaultPicture.png");
      }
      else if ( IsPlayList() )
      {
        SetIconImage("DefaultPlaylist.png");
      }
      else if ( IsXBE() )
      {
        // xbe
        SetIconImage("DefaultProgram.png");
      }
      else if ( IsShortCut() && !IsLabelPreformated() )
      {
        // shortcut
        CStdString strFName = CUtil::GetFileName(m_strPath);
        int iPos = strFName.ReverseFind(".");
        CStdString strDescription = strFName.Left(iPos);
        SetLabel(strDescription);
        SetIconImage("DefaultShortcut.png");
      }
      else if ( IsPythonScript() )
      {
        SetIconImage("DefaultScript.png");
      }
      else
      {
        // default icon for unknown file type
        SetIconImage("DefaultFile.png");
      }
    }
    else
    {
      if ( IsPlayList() )
      {
        SetIconImage("DefaultPlaylist.png");
      }
      else if (IsParentFolder())
      {
        SetIconImage("DefaultFolderBack.png");
      }
      else
      {
        SetIconImage("DefaultFolder.png");
      }
    }
  }
  // Set the icon overlays (if applicable)
  if (!HasOverlay())
  {
    if (CUtil::IsInRAR(m_strPath))
      SetOverlayImage(CGUIListItem::ICON_OVERLAY_RAR);
    else if (CUtil::IsInZIP(m_strPath))
      SetOverlayImage(CGUIListItem::ICON_OVERLAY_ZIP);
  }
}

CStdString CFileItem::GetCachedArtistThumb() const
{
  return GetCachedThumb("artist"+GetLabel(),g_settings.GetMusicArtistThumbFolder());
}

CStdString CFileItem::GetCachedSeasonThumb() const
{
  CStdString seasonPath;
  if (HasVideoInfoTag())
    seasonPath = GetVideoInfoTag()->m_strPath;

  return GetCachedThumb("season"+seasonPath+GetLabel(),g_settings.GetVideoThumbFolder(),true);
}

CStdString CFileItem::GetCachedActorThumb() const
{
  return GetCachedThumb("actor"+GetLabel(),g_settings.GetVideoThumbFolder(),true);
}

void CFileItem::SetCachedArtistThumb()
{
  CStdString thumb(GetCachedArtistThumb());
  if (CFile::Exists(thumb))
  {
    // found it, we are finished.
    SetThumbnailImage(thumb);
  }
}

// set the album thumb for a file or folder
void CFileItem::SetMusicThumb(bool alwaysCheckRemote /* = true */)
{
  if (HasThumbnail()) return;

  SetCachedMusicThumb();
  if (!HasThumbnail())
    SetUserMusicThumb(alwaysCheckRemote);
}

void CFileItem::SetCachedSeasonThumb()
{
  CStdString thumb(GetCachedSeasonThumb());
  if (CFile::Exists(thumb))
  {
    // found it, we are finished.
    SetThumbnailImage(thumb);
  }
}

void CFileItem::RemoveExtension()
{
  if (m_bIsFolder)
    return;
  CStdString strLabel = GetLabel();
  CUtil::RemoveExtension(strLabel);
  SetLabel(strLabel);
}

void CFileItem::CleanString()
{
  if (IsLiveTV())
    return;

  bool bIsFolder = m_bIsFolder;

  // make sure we don't append the extension to stacked dvd folders
  if (HasProperty("isstacked") && IsDVDFile(false, true))
    bIsFolder = true;

  CStdString strLabel = GetLabel();
  CStdString strTitle, strTitleAndYear, strYear;
  CUtil::CleanString(strLabel, strTitle, strTitleAndYear, strYear, (bIsFolder || !g_guiSettings.GetBool("filelists.showextensions") ) );
  SetLabel(strTitleAndYear);
}

void CFileItem::SetLabel(const CStdString &strLabel)
{
  if (strLabel=="..")
  {
    m_bIsParentFolder=true;
    m_bIsFolder=true;
    m_specialSort = SORT_ON_TOP;
    SetLabelPreformated(true);
  }
  CGUIListItem::SetLabel(strLabel);
}

void CFileItem::SetFileSizeLabel()
{
  if( m_bIsFolder && m_dwSize == 0 )
    SetLabel2("");
  else
    SetLabel2(StringUtils::SizeToString(m_dwSize));
}

CURL CFileItem::GetAsUrl() const
{
  return CURL(m_strPath);
}

bool CFileItem::CanQueue() const
{
  return m_bCanQueue;
}

void CFileItem::SetCanQueue(bool bYesNo)
{
  m_bCanQueue=bYesNo;
}

bool CFileItem::IsParentFolder() const
{
  return m_bIsParentFolder;
}

const CStdString& CFileItem::GetMimeType(bool lookup /*= true*/) const
{
  if( m_mimetype.IsEmpty() && lookup)
  {
    // discard const qualifyier
    CStdString& m_ref = (CStdString&)m_mimetype;

    if( m_bIsFolder )
      m_ref = "x-directory/normal";
    else if( m_strPath.Left(8).Equals("shout://")
          || m_strPath.Left(7).Equals("http://")
          || m_strPath.Left(8).Equals("https://"))
    {
      CFileCurl::GetMimeType(GetAsUrl(), m_ref);

      // try to get mime-type again but with an NSPlayer User-Agent
      // in order for server to provide correct mime-type.  Allows us
      // to properly detect an MMS stream
      if (m_ref.Left(11).Equals("video/x-ms-"))
        CFileCurl::GetMimeType(GetAsUrl(), m_ref, "NSPlayer/11.00.6001.7000");

      // make sure there are no options set in mime-type
      // mime-type can look like "video/x-ms-asf ; charset=utf8"
      int i = m_ref.Find(';');
      if(i>=0)
        m_ref.Delete(i,m_ref.length()-i);
      m_ref.Trim();
    }

    // if it's still empty set to an unknown type
    if( m_ref.IsEmpty() )
      m_ref = "application/octet-stream";
  }

  // change protocol to mms for the following mome-type.  Allows us to create proper FileMMS.
  if( m_mimetype.Left(32).Equals("application/vnd.ms.wms-hdr.asfv1") || m_mimetype.Left(24).Equals("application/x-mms-framed") )
  {
    CStdString& m_path = (CStdString&)m_strPath;
    m_path.Replace("http:", "mms:");
  }

  return m_mimetype;
}

bool CFileItem::IsSamePath(const CFileItem *item) const
{
  if (!item)
    return false;

  if (item->m_strPath == m_strPath && item->m_lStartOffset == m_lStartOffset) return true;
  if (IsMusicDb() && HasMusicInfoTag())
  {
    CFileItem dbItem(m_musicInfoTag->GetURL(), false);
    dbItem.m_lStartOffset = m_lStartOffset;
    return dbItem.IsSamePath(item);
  }
  if (IsVideoDb() && HasVideoInfoTag())
  {
    CFileItem dbItem(m_videoInfoTag->m_strFileNameAndPath, false);
    dbItem.m_lStartOffset = m_lStartOffset;
    return dbItem.IsSamePath(item);
  }
  if (item->IsMusicDb() && item->HasMusicInfoTag())
  {
    CFileItem dbItem(item->m_musicInfoTag->GetURL(), false);
    dbItem.m_lStartOffset = item->m_lStartOffset;
    return IsSamePath(&dbItem);
  }
  if (item->IsVideoDb() && item->HasVideoInfoTag())
  {
    CFileItem dbItem(item->m_videoInfoTag->m_strFileNameAndPath, false);
    dbItem.m_lStartOffset = item->m_lStartOffset;
    return IsSamePath(&dbItem);
  }
  if (HasProperty("original_listitem_url"))
    return (GetProperty("original_listitem_url") == item->m_strPath);
  return false;
}

bool CFileItem::IsAlbum() const
{
  return m_bIsAlbum;
}

/////////////////////////////////////////////////////////////////////////////////
/////
///// CFileItemList
/////
//////////////////////////////////////////////////////////////////////////////////

CFileItemList::CFileItemList()
{
  m_fastLookup = false;
  m_bIsFolder=true;
  m_cacheToDisc=CACHE_IF_SLOW;
  m_sortMethod=SORT_METHOD_NONE;
  m_sortOrder=SORT_ORDER_NONE;
  m_replaceListing = false;
}

CFileItemList::CFileItemList(const CStdString& strPath)
{
  m_strPath=strPath;
  m_fastLookup = false;
  m_bIsFolder=true;
  m_cacheToDisc=CACHE_IF_SLOW;
  m_sortMethod=SORT_METHOD_NONE;
  m_sortOrder=SORT_ORDER_NONE;
  m_replaceListing = false;
}

CFileItemList::~CFileItemList()
{
  Clear();
}

CFileItemPtr CFileItemList::operator[] (int iItem)
{
  return Get(iItem);
}

const CFileItemPtr CFileItemList::operator[] (int iItem) const
{
  return Get(iItem);
}

CFileItemPtr CFileItemList::operator[] (const CStdString& strPath)
{
  return Get(strPath);
}

const CFileItemPtr CFileItemList::operator[] (const CStdString& strPath) const
{
  return Get(strPath);
}

void CFileItemList::SetFastLookup(bool fastLookup)
{
  CSingleLock lock(m_lock);

  if (fastLookup && !m_fastLookup)
  { // generate the map
    m_map.clear();
    for (unsigned int i=0; i < m_items.size(); i++)
    {
      CFileItemPtr pItem = m_items[i];
      CStdString path(pItem->m_strPath); path.ToLower();
      m_map.insert(MAPFILEITEMSPAIR(path, pItem));
    }
  }
  if (!fastLookup && m_fastLookup)
    m_map.clear();
  m_fastLookup = fastLookup;
}

bool CFileItemList::Contains(const CStdString& fileName) const
{
  CSingleLock lock(m_lock);

  // checks case insensitive
  CStdString checkPath(fileName); checkPath.ToLower();
  if (m_fastLookup)
    return m_map.find(checkPath) != m_map.end();
  // slow method...
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    const CFileItemPtr pItem = m_items[i];
    if (pItem->m_strPath.Equals(checkPath))
      return true;
  }
  return false;
}

void CFileItemList::Clear()
{
  CSingleLock lock(m_lock);

  ClearItems();
  m_sortMethod=SORT_METHOD_NONE;
  m_sortOrder=SORT_ORDER_NONE;
  m_cacheToDisc=CACHE_IF_SLOW;
  m_sortDetails.clear();
  m_replaceListing = false;
  m_content.Empty();
}

void CFileItemList::ClearItems()
{
  CSingleLock lock(m_lock);
  // make sure we free the memory of the items (these are GUIControls which may have allocated resources)
  FreeMemory();
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    CFileItemPtr item = m_items[i];
    item->FreeMemory();
  }
  m_items.clear();
  m_map.clear();
}

void CFileItemList::Add(const CFileItemPtr &pItem)
{
  CSingleLock lock(m_lock);

  m_items.push_back(pItem);
  if (m_fastLookup)
  {
    CStdString path(pItem->m_strPath); path.ToLower();
    m_map.insert(MAPFILEITEMSPAIR(path, pItem));
  }
}

void CFileItemList::AddFront(const CFileItemPtr &pItem, int itemPosition)
{
  CSingleLock lock(m_lock);

  if (itemPosition >= 0)
  {
    m_items.insert(m_items.begin()+itemPosition, pItem);
  }
  else
  {
    m_items.insert(m_items.begin()+(m_items.size()+itemPosition), pItem);
  }
  if (m_fastLookup)
  {
    CStdString path(pItem->m_strPath); path.ToLower();
    m_map.insert(MAPFILEITEMSPAIR(path, pItem));
  }
}

void CFileItemList::Remove(CFileItem* pItem)
{
  CSingleLock lock(m_lock);

  for (IVECFILEITEMS it = m_items.begin(); it != m_items.end(); ++it)
  {
    if (pItem == it->get())
    {
      m_items.erase(it);
      if (m_fastLookup)
      {
        CStdString path(pItem->m_strPath); path.ToLower();
        m_map.erase(path);
      }
      break;
    }
  }
}

void CFileItemList::Remove(int iItem)
{
  CSingleLock lock(m_lock);

  if (iItem >= 0 && iItem < (int)Size())
  {
    CFileItemPtr pItem = *(m_items.begin() + iItem);
    if (m_fastLookup)
    {
      CStdString path(pItem->m_strPath); path.ToLower();
      m_map.erase(path);
    }
    m_items.erase(m_items.begin() + iItem);
  }
}

void CFileItemList::Append(const CFileItemList& itemlist)
{
  CSingleLock lock(m_lock);

  for (int i = 0; i < itemlist.Size(); ++i)
    Add(itemlist[i]);
}

void CFileItemList::Assign(const CFileItemList& itemlist, bool append)
{
  CSingleLock lock(m_lock);
  if (!append)
    Clear();
  Append(itemlist);
  m_strPath = itemlist.m_strPath;
  m_sortDetails = itemlist.m_sortDetails;
  m_replaceListing = itemlist.m_replaceListing;
  m_content = itemlist.m_content;
  m_mapProperties = itemlist.m_mapProperties;
  m_cacheToDisc = itemlist.m_cacheToDisc;
}

bool CFileItemList::Copy(const CFileItemList& items)
{
  // assign all CFileItem parts
  *(CFileItem*)this = *(CFileItem*)&items;

  // assign the rest of the CFileItemList properties
  m_replaceListing = items.m_replaceListing;
  m_content        = items.m_content;
  m_mapProperties  = items.m_mapProperties;
  m_cacheToDisc    = items.m_cacheToDisc;
  m_sortDetails    = items.m_sortDetails;
  m_sortMethod     = items.m_sortMethod;
  m_sortOrder      = items.m_sortOrder;

  // make a copy of each item
  for (int i = 0; i < items.Size(); i++)
  {
    CFileItemPtr newItem(new CFileItem(*items[i]));
    Add(newItem);
  }

  return true;
}

CFileItemPtr CFileItemList::Get(int iItem)
{
  CSingleLock lock(m_lock);

  if (iItem > -1 && iItem < (int)m_items.size())
    return m_items[iItem];

  return CFileItemPtr();
}

const CFileItemPtr CFileItemList::Get(int iItem) const
{
  CSingleLock lock(m_lock);

  if (iItem > -1 && iItem < (int)m_items.size())
    return m_items[iItem];

  return CFileItemPtr();
}

CFileItemPtr CFileItemList::Get(const CStdString& strPath)
{
  CSingleLock lock(m_lock);

  CStdString pathToCheck(strPath); pathToCheck.ToLower();
  if (m_fastLookup)
  {
    IMAPFILEITEMS it=m_map.find(pathToCheck);
    if (it != m_map.end())
      return it->second;

    return CFileItemPtr();
  }
  // slow method...
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->m_strPath.Equals(pathToCheck))
      return pItem;
  }

  return CFileItemPtr();
}

const CFileItemPtr CFileItemList::Get(const CStdString& strPath) const
{
  CSingleLock lock(m_lock);

  CStdString pathToCheck(strPath); pathToCheck.ToLower();
  if (m_fastLookup)
  {
    map<CStdString, CFileItemPtr>::const_iterator it=m_map.find(pathToCheck);
    if (it != m_map.end())
      return it->second;

    return CFileItemPtr();
  }
  // slow method...
  for (unsigned int i = 0; i < m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->m_strPath.Equals(pathToCheck))
      return pItem;
  }

  return CFileItemPtr();
}

int CFileItemList::Size() const
{
  CSingleLock lock(m_lock);
  return (int)m_items.size();
}

bool CFileItemList::IsEmpty() const
{
  CSingleLock lock(m_lock);
  return (m_items.size() <= 0);
}

void CFileItemList::Reserve(int iCount)
{
  CSingleLock lock(m_lock);
  m_items.reserve(iCount);
}

void CFileItemList::Sort(FILEITEMLISTCOMPARISONFUNC func)
{
  CSingleLock lock(m_lock);
  std::stable_sort(m_items.begin(), m_items.end(), func);
}

void CFileItemList::FillSortFields(FILEITEMFILLFUNC func)
{
  CSingleLock lock(m_lock);
  std::for_each(m_items.begin(), m_items.end(), func);
}

void CFileItemList::Sort(SORT_METHOD sortMethod, SORT_ORDER sortOrder)
{
  //  Already sorted?
  if (sortMethod==m_sortMethod && m_sortOrder==sortOrder)
    return;

  switch (sortMethod)
  {
  case SORT_METHOD_LABEL:
  case SORT_METHOD_LABEL_IGNORE_FOLDERS:
    FillSortFields(SSortFileItem::ByLabel);
    break;
  case SORT_METHOD_LABEL_IGNORE_THE:
    FillSortFields(SSortFileItem::ByLabelNoThe);
    break;
  case SORT_METHOD_DATE:
    FillSortFields(SSortFileItem::ByDate);
    break;
  case SORT_METHOD_SIZE:
    FillSortFields(SSortFileItem::BySize);
    break;
  case SORT_METHOD_BITRATE:
    FillSortFields(SSortFileItem::ByBitrate);
    break;      
  case SORT_METHOD_DRIVE_TYPE:
    FillSortFields(SSortFileItem::ByDriveType);
    break;
  case SORT_METHOD_TRACKNUM:
    FillSortFields(SSortFileItem::BySongTrackNum);
    break;
  case SORT_METHOD_EPISODE:
    FillSortFields(SSortFileItem::ByEpisodeNum);
    break;
  case SORT_METHOD_DURATION:
    FillSortFields(SSortFileItem::BySongDuration);
    break;
  case SORT_METHOD_TITLE_IGNORE_THE:
    FillSortFields(SSortFileItem::BySongTitleNoThe);
    break;
  case SORT_METHOD_TITLE:
    FillSortFields(SSortFileItem::BySongTitle);
    break;
  case SORT_METHOD_ARTIST:
    FillSortFields(SSortFileItem::BySongArtist);
    break;
  case SORT_METHOD_ARTIST_IGNORE_THE:
    FillSortFields(SSortFileItem::BySongArtistNoThe);
    break;
  case SORT_METHOD_ALBUM:
    FillSortFields(SSortFileItem::BySongAlbum);
    break;
  case SORT_METHOD_ALBUM_IGNORE_THE:
    FillSortFields(SSortFileItem::BySongAlbumNoThe);
    break;
  case SORT_METHOD_GENRE:
    FillSortFields(SSortFileItem::ByGenre);
    break;
  case SORT_METHOD_COUNTRY:
    FillSortFields(SSortFileItem::ByCountry);
    break;
  case SORT_METHOD_DATEADDED:
    FillSortFields(SSortFileItem::ByDateAdded);
    break;
  case SORT_METHOD_FILE:
    FillSortFields(SSortFileItem::ByFile);
    break;
  case SORT_METHOD_VIDEO_RATING:
    FillSortFields(SSortFileItem::ByMovieRating);
    break;
  case SORT_METHOD_VIDEO_TITLE:
    FillSortFields(SSortFileItem::ByMovieTitle);
    break;
  case SORT_METHOD_VIDEO_SORT_TITLE:
    FillSortFields(SSortFileItem::ByMovieSortTitle);
    break;
  case SORT_METHOD_VIDEO_SORT_TITLE_IGNORE_THE:
    FillSortFields(SSortFileItem::ByMovieSortTitleNoThe);
    break;
  case SORT_METHOD_YEAR:
    FillSortFields(SSortFileItem::ByYear);
    break;
  case SORT_METHOD_PRODUCTIONCODE:
    FillSortFields(SSortFileItem::ByProductionCode);
    break;
  case SORT_METHOD_PROGRAM_COUNT:
  case SORT_METHOD_PLAYLIST_ORDER:
    // TODO: Playlist order is hacked into program count variable (not nice, but ok until 2.0)
    FillSortFields(SSortFileItem::ByProgramCount);
    break;
  case SORT_METHOD_SONG_RATING:
    FillSortFields(SSortFileItem::BySongRating);
    break;
  case SORT_METHOD_MPAA_RATING:
    FillSortFields(SSortFileItem::ByMPAARating);
    break;
  case SORT_METHOD_VIDEO_RUNTIME:
    FillSortFields(SSortFileItem::ByMovieRuntime);
    break;
  case SORT_METHOD_STUDIO:
    FillSortFields(SSortFileItem::ByStudio);
    break;
  case SORT_METHOD_STUDIO_IGNORE_THE:
    FillSortFields(SSortFileItem::ByStudioNoThe);
    break;
  case SORT_METHOD_FULLPATH:
    FillSortFields(SSortFileItem::ByFullPath);
    break;
  case SORT_METHOD_LASTPLAYED:
    FillSortFields(SSortFileItem::ByLastPlayed);
    break;
  default:
    break;
  }
  if (sortMethod == SORT_METHOD_FILE        ||
      sortMethod == SORT_METHOD_VIDEO_SORT_TITLE ||
      sortMethod == SORT_METHOD_VIDEO_SORT_TITLE_IGNORE_THE ||
      sortMethod == SORT_METHOD_LABEL_IGNORE_FOLDERS)
    Sort(sortOrder==SORT_ORDER_ASC ? SSortFileItem::IgnoreFoldersAscending : SSortFileItem::IgnoreFoldersDescending);
  else if (sortMethod != SORT_METHOD_NONE && sortMethod != SORT_METHOD_UNSORTED)
    Sort(sortOrder==SORT_ORDER_ASC ? SSortFileItem::Ascending : SSortFileItem::Descending);

  m_sortMethod=sortMethod;
  m_sortOrder=sortOrder;
}

void CFileItemList::Randomize()
{
  CSingleLock lock(m_lock);
  random_shuffle(m_items.begin(), m_items.end());
}

void CFileItemList::Serialize(CArchive& ar)
{
  CSingleLock lock(m_lock);
  if (ar.IsStoring())
  {
    CFileItem::Serialize(ar);

    int i = 0;
    if (m_items.size() > 0 && m_items[0]->IsParentFolder())
      i = 1;

    ar << (int)(m_items.size() - i);

    ar << m_fastLookup;

    ar << (int)m_sortMethod;
    ar << (int)m_sortOrder;
    ar << (int)m_cacheToDisc;

    ar << (int)m_sortDetails.size();
    for (unsigned int j = 0; j < m_sortDetails.size(); ++j)
    {
      const SORT_METHOD_DETAILS &details = m_sortDetails[j];
      ar << (int)details.m_sortMethod;
      ar << details.m_buttonLabel;
      ar << details.m_labelMasks.m_strLabelFile;
      ar << details.m_labelMasks.m_strLabelFolder;
      ar << details.m_labelMasks.m_strLabel2File;
      ar << details.m_labelMasks.m_strLabel2Folder;
    }

    ar << m_content;

    for (; i < (int)m_items.size(); ++i)
    {
      CFileItemPtr pItem = m_items[i];
      ar << *pItem;
    }
  }
  else
  {
    CFileItemPtr pParent;
    if (!IsEmpty())
    {
      CFileItemPtr pItem=m_items[0];
      if (pItem->IsParentFolder())
        pParent.reset(new CFileItem(*pItem));
    }

    SetFastLookup(false);
    Clear();


    CFileItem::Serialize(ar);

    int iSize = 0;
    ar >> iSize;
    if (iSize <= 0)
      return ;

    if (pParent)
    {
      m_items.reserve(iSize + 1);
      m_items.push_back(pParent);
    }
    else
      m_items.reserve(iSize);

    bool fastLookup=false;
    ar >> fastLookup;

    int tempint;
    ar >> (int&)tempint;
    m_sortMethod = SORT_METHOD(tempint);
    ar >> (int&)tempint;
    m_sortOrder = SORT_ORDER(tempint);
    ar >> (int&)tempint;
    m_cacheToDisc = CACHE_TYPE(tempint);

    unsigned int detailSize = 0;
    ar >> detailSize;
    for (unsigned int j = 0; j < detailSize; ++j)
    {
      SORT_METHOD_DETAILS details;
      ar >> (int&)tempint;
      details.m_sortMethod = SORT_METHOD(tempint);
      ar >> details.m_buttonLabel;
      ar >> details.m_labelMasks.m_strLabelFile;
      ar >> details.m_labelMasks.m_strLabelFolder;
      ar >> details.m_labelMasks.m_strLabel2File;
      ar >> details.m_labelMasks.m_strLabel2Folder;
      m_sortDetails.push_back(details);
    }

    ar >> m_content;

    for (int i = 0; i < iSize; ++i)
    {
      CFileItemPtr pItem(new CFileItem);
      ar >> *pItem;
      Add(pItem);
    }

    SetFastLookup(fastLookup);
  }
}

void CFileItemList::FillInDefaultIcons()
{
  CSingleLock lock(m_lock);
  for (int i = 0; i < (int)m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->FillInDefaultIcon();
  }
}

void CFileItemList::SetMusicThumbs()
{
  CSingleLock lock(m_lock);
  //cache thumbnails directory
  g_directoryCache.InitMusicThumbCache();

  for (int i = 0; i < (int)m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->SetMusicThumb();
  }

  g_directoryCache.ClearMusicThumbCache();
}

int CFileItemList::GetFolderCount() const
{
  CSingleLock lock(m_lock);
  int nFolderCount = 0;
  for (int i = 0; i < (int)m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->m_bIsFolder)
      nFolderCount++;
  }

  return nFolderCount;
}

int CFileItemList::GetObjectCount() const
{
  CSingleLock lock(m_lock);

  int numObjects = (int)m_items.size();
  if (numObjects && m_items[0]->IsParentFolder())
    numObjects--;

  return numObjects;
}

int CFileItemList::GetFileCount() const
{
  CSingleLock lock(m_lock);
  int nFileCount = 0;
  for (int i = 0; i < (int)m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (!pItem->m_bIsFolder)
      nFileCount++;
  }

  return nFileCount;
}

int CFileItemList::GetSelectedCount() const
{
  CSingleLock lock(m_lock);
  int count = 0;
  for (int i = 0; i < (int)m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (pItem->IsSelected())
      count++;
  }

  return count;
}

void CFileItemList::FilterCueItems()
{
  CSingleLock lock(m_lock);
  // Handle .CUE sheet files...
  VECSONGS itemstoadd;
  CStdStringArray itemstodelete;
  for (int i = 0; i < (int)m_items.size(); i++)
  {
    CFileItemPtr pItem = m_items[i];
    if (!pItem->m_bIsFolder)
    { // see if it's a .CUE sheet
      if (pItem->IsCUESheet())
      {
        CCueDocument cuesheet;
        if (cuesheet.Parse(pItem->m_strPath))
        {
          VECSONGS newitems;
          cuesheet.GetSongs(newitems);

          std::vector<CStdString> MediaFileVec;
          cuesheet.GetMediaFiles(MediaFileVec);

          // queue the cue sheet and the underlying media file for deletion
          for(std::vector<CStdString>::iterator itMedia = MediaFileVec.begin(); itMedia != MediaFileVec.end(); itMedia++)
          {
            CStdString strMediaFile = *itMedia;
            CStdString fileFromCue = strMediaFile; // save the file from the cue we're matching against,
                                                   // as we're going to search for others here...
            bool bFoundMediaFile = CFile::Exists(strMediaFile);
            // queue the cue sheet and the underlying media file for deletion
            if (!bFoundMediaFile)
            {
              // try file in same dir, not matching case...
              if (Contains(strMediaFile))
              {
                bFoundMediaFile = true;
              }
              else
              {
                // try removing the .cue extension...
                strMediaFile = pItem->m_strPath;
                CUtil::RemoveExtension(strMediaFile);
                CFileItem item(strMediaFile, false);
                if (item.IsAudio() && Contains(strMediaFile))
                {
                  bFoundMediaFile = true;
                }
                else
                { // try replacing the extension with one of our allowed ones.
                  CStdStringArray extensions;
                  StringUtils::SplitString(g_settings.m_musicExtensions, "|", extensions);
                  for (unsigned int i = 0; i < extensions.size(); i++)
                  {
                    strMediaFile = CUtil::ReplaceExtension(pItem->m_strPath, extensions[i]);
                    CFileItem item(strMediaFile, false);
                    if (!item.IsCUESheet() && !item.IsPlayList() && Contains(strMediaFile))
                    {
                      bFoundMediaFile = true;
                      break;
                    }
                  }
                }
              }
            }
            if (bFoundMediaFile)
            {
              itemstodelete.push_back(pItem->m_strPath);
              itemstodelete.push_back(strMediaFile);
              // get the additional stuff (year, genre etc.) from the underlying media files tag.
              CMusicInfoTag tag;
              auto_ptr<IMusicInfoTagLoader> pLoader (CMusicInfoTagLoaderFactory::CreateLoader(strMediaFile));
              if (NULL != pLoader.get())
              {
                // get id3tag
                pLoader->Load(strMediaFile, tag);
              }
              // fill in any missing entries from underlying media file
              for (int j = 0; j < (int)newitems.size(); j++)
              {
                CSong song = newitems[j];
                // only for songs that actually match the current media file
                if (song.strFileName == fileFromCue)
                {
                  // we might have a new media file from the above matching code
                  song.strFileName = strMediaFile;
                  if (tag.Loaded())
                  {
                    if (song.strAlbum.empty() && !tag.GetAlbum().empty()) song.strAlbum = tag.GetAlbum();
                    if (song.strAlbumArtist.empty() && !tag.GetAlbumArtist().empty()) song.strAlbumArtist = tag.GetAlbumArtist();
                    if (song.strGenre.empty() && !tag.GetGenre().empty()) song.strGenre = tag.GetGenre();
                    if (song.strArtist.empty() && !tag.GetArtist().empty()) song.strArtist = tag.GetArtist();
                    if (tag.GetDiscNumber()) song.iTrack |= (tag.GetDiscNumber() << 16); // see CMusicInfoTag::GetDiscNumber()
                    SYSTEMTIME dateTime;
                    tag.GetReleaseDate(dateTime);
                    if (dateTime.wYear) song.iYear = dateTime.wYear;
                  }
                  if (!song.iDuration && tag.GetDuration() > 0)
                  { // must be the last song
                    song.iDuration = (tag.GetDuration() * 75 - song.iStartOffset + 37) / 75;
                  }
                  // add this item to the list
                  itemstoadd.push_back(song);
                }
              }
            }
            else
            { // remove the .cue sheet from the directory
              itemstodelete.push_back(pItem->m_strPath);
            }
          }
        }
        else
        { // remove the .cue sheet from the directory (can't parse it - no point listing it)
          itemstodelete.push_back(pItem->m_strPath);
        }
      }
    }
  }
  // now delete the .CUE files and underlying media files.
  for (int i = 0; i < (int)itemstodelete.size(); i++)
  {
    for (int j = 0; j < (int)m_items.size(); j++)
    {
      CFileItemPtr pItem = m_items[j];
      if (stricmp(pItem->m_strPath.c_str(), itemstodelete[i].c_str()) == 0)
      { // delete this item
        m_items.erase(m_items.begin() + j);
        break;
      }
    }
  }
  // and add the files from the .CUE sheet
  for (int i = 0; i < (int)itemstoadd.size(); i++)
  {
    // now create the file item, and add to the item list.
    CFileItemPtr pItem(new CFileItem(itemstoadd[i]));
    m_items.push_back(pItem);
  }
}

// Remove the extensions from the filenames
void CFileItemList::RemoveExtensions()
{
  CSingleLock lock(m_lock);
  for (int i = 0; i < Size(); ++i)
    m_items[i]->RemoveExtension();
}

void CFileItemList::Stack()
{
  CSingleLock lock(m_lock);

  // not allowed here
  if (IsVirtualDirectoryRoot() || IsLiveTV())
    return;

  SetProperty("isstacked", "1");

  // items needs to be sorted for stuff below to work properly
  Sort(SORT_METHOD_LABEL, SORT_ORDER_ASC);

  // stack folders
  bool isDVDFolder(false);
  int i = 0;
  for (i = 0; i < Size(); ++i)
  {
    CFileItemPtr item = Get(i);
    if (item->GetLabel().Equals("VIDEO_TS.IFO"))
    {
      isDVDFolder = true;
      break;
    }
    // combined the folder checks
    if (item->m_bIsFolder)
    {
      // only check known fast sources?
      // xbms included because it supports file existance
      // NOTES:
      // 1. xbms would not have worked previously: item->m_strPath.Left(5).Equals("xbms", false)
      // 2. rars and zips may be on slow sources? is this supposed to be allowed?
      if( !item->IsRemote()
        || item->IsSmb()
        || item->IsXBMS()
        || CUtil::IsInRAR(item->m_strPath)
        || CUtil::IsInZIP(item->m_strPath)
        )
      {
        // stack cd# folders if contains only a single video file
        // NOTE: if we're doing this anyway, why not collapse *all* folders with just a single video file?
        CStdString folderName = item->GetLabel();
        if (folderName.Left(2).Equals("CD") && StringUtils::IsNaturalNumber(folderName.Mid(2)))
        {
          CFileItemList items;
          CDirectory::GetDirectory(item->m_strPath,items,g_settings.m_videoExtensions,true);
          // optimized to only traverse listing once by checking for filecount
          // and recording last file item for later use
          int nFiles = 0;
          int index = -1;
          for (int j = 0; j < items.Size(); j++)
          {
            if (!items[j]->m_bIsFolder)
            {
              nFiles++;
              index = j;
            }
            if (nFiles > 1)
              break;
          }
          if (nFiles == 1)
          {
            *item = *items[index];
          }
        }

        // check for dvd folders
        else
        {
          CStdString path;
          CStdString dvdPath;
          CUtil::AddFileToFolder(item->m_strPath, "VIDEO_TS.IFO", path);
          if (CFile::Exists(path))
            dvdPath = path;
          else
          {
            CUtil::AddFileToFolder(item->m_strPath, "VIDEO_TS", dvdPath);
            CUtil::AddFileToFolder(dvdPath, "VIDEO_TS.IFO", path);
            dvdPath.Empty();
            if (CFile::Exists(path))
              dvdPath = path;
          }
#ifdef HAVE_LIBBLURAY
          if (dvdPath.IsEmpty())
          {
            CUtil::AddFileToFolder(item->m_strPath, "index.bdmv", path);
            if (CFile::Exists(path))
              dvdPath = path;
            else
            {
              CUtil::AddFileToFolder(item->m_strPath, "BDMV", dvdPath);
              CUtil::AddFileToFolder(dvdPath, "index.bdmv", path);
              dvdPath.Empty();
              if (CFile::Exists(path))
                dvdPath = path;
            }
          }
#endif
          if (!dvdPath.IsEmpty())
          {
            // NOTE: should this be done for the CD# folders too?
            /* set the thumbnail based on folder */
            item->SetCachedVideoThumb();
            if (!item->HasThumbnail())
              item->SetUserVideoThumb();

            item->m_bIsFolder = false;
            item->m_strPath = dvdPath;
            item->SetLabel2("");
            item->SetLabelPreformated(true);
            m_sortMethod = SORT_METHOD_NONE; /* sorting is now broken */

            /* override the previously set thumb if video_ts.ifo has any */
            /* otherwise user can't set icon on the stacked file as that */
            /* will allways be set on the video_ts.ifo file */
            CStdString thumb(item->GetCachedVideoThumb());
            if(CFile::Exists(thumb))
              item->SetThumbnailImage(thumb);
            else
              item->SetUserVideoThumb();
          }
        }
      }
    }
  }

  // Precompile our REs
  VECCREGEXP stackRegExps;
  CRegExp tmpRegExp(true);
  const CStdStringArray& strStackRegExps = g_advancedSettings.m_videoStackRegExps;
  CStdStringArray::const_iterator strRegExp = strStackRegExps.begin();
  while (strRegExp != strStackRegExps.end())
  {
    if (tmpRegExp.RegComp(*strRegExp))
    {
      if (tmpRegExp.GetCaptureTotal() == 4)
        stackRegExps.push_back(tmpRegExp);
      else
        CLog::Log(LOGERROR, "Invalid video stack RE (%s). Must have 4 captures.", strRegExp->c_str());
    }
    strRegExp++;
  }

  // now stack the files, some of which may be from the previous stack iteration
  i = 0;
  while (i < Size())
  {
    CFileItemPtr item1 = Get(i);

    // set property
    item1->SetProperty("isstacked", "1");

    // skip folders, nfo files, playlists
    if (item1->m_bIsFolder
      || item1->IsParentFolder()
      || item1->IsNFO()
      || item1->IsPlayList()
      )
    {
      // increment index
      i++;
      continue;
    }

    if (isDVDFolder)
    {
      // remove any other ifo files in this folder
      if (item1->IsDVDFile(false, true) && item1->GetLabel().Equals("VIDEO_TS.IFO"))
      {
        Remove(i);
        continue;
      }
    }

    int64_t               size        = 0;
    size_t                offset      = 0;
    CStdString            stackName;
    CStdString            file1;
    CStdString            filePath;
    vector<int>           stack;
    VECCREGEXP::iterator  expr        = stackRegExps.begin();

    CUtil::Split(item1->m_strPath, filePath, file1);
    int j;
    while (expr != stackRegExps.end())
    {
      if (expr->RegFind(file1, offset) != -1)
      {
        CStdString  Title1      = expr->GetMatch(1),
                    Volume1     = expr->GetMatch(2),
                    Ignore1     = expr->GetMatch(3),
                    Extension1  = expr->GetMatch(4);
        if (offset)
          Title1 = file1.substr(0, expr->GetSubStart(2));
        j = i + 1;
        while (j < Size())
        {
          CFileItemPtr item2 = Get(j);

          // skip folders, nfo files, playlists
          if (item2->m_bIsFolder
            || item2->IsParentFolder()
            || item2->IsNFO()
            || item2->IsPlayList()
            )
          {
            // increment index
            j++;
            continue;
          }

          if (isDVDFolder)
          {
            // remove any other ifo files in this folder
            if (item2->IsDVDFile(false, true) && item2->GetLabel().Equals("VIDEO_TS.IFO"))
            {
              Remove(j);
              continue;
            }
          }

          CStdString file2, filePath2;
          CUtil::Split(item2->m_strPath, filePath2, file2);

          if (expr->RegFind(file2, offset) != -1)
          {
            CStdString  Title2      = expr->GetMatch(1),
                        Volume2     = expr->GetMatch(2),
                        Ignore2     = expr->GetMatch(3),
                        Extension2  = expr->GetMatch(4);
            if (offset)
              Title2 = file2.substr(0, expr->GetSubStart(2));
            if (Title1.Equals(Title2))
            {
              if (!Volume1.Equals(Volume2))
              {
                if (Ignore1.Equals(Ignore2) && Extension1.Equals(Extension2))
                {
                  if (stack.size() == 0)
                  {
                    stackName = Title1 + Ignore1 + Extension1;
                    stack.push_back(i);
                    size += item1->m_dwSize;
                  }
                  stack.push_back(j);
                  size += item2->m_dwSize;
                }
                else // Sequel
                {
                  offset = 0;
                  expr++;
                  break;
                }
              }
              else if (!Ignore1.Equals(Ignore2)) // False positive, try again with offset
              {
                offset = expr->GetSubStart(3);
                break;
              }
              else // Extension mismatch
              {
                offset = 0;
                expr++;
                break;
              }
            }
            else // Title mismatch
            {
              offset = 0;
              expr++;
              break;
            }
          }
          else // No match 2, next expression
          {
            offset = 0;
            expr++;
            break;
          }
          j++;
        }
        if (j == Size())
          expr = stackRegExps.end();
      }
      else // No match 1
      {
        offset = 0;
        expr++;
      }
      if (stack.size() > 1)
      {
        // have a stack, remove the items and add the stacked item
        // dont actually stack a multipart rar set, just remove all items but the first
        CStdString stackPath;
        if (Get(stack[0])->IsRAR())
          stackPath = Get(stack[0])->m_strPath;
        else
        {
          CStackDirectory dir;
          stackPath = dir.ConstructStackPath(*this, stack);
        }
        item1->m_strPath = stackPath;
        // clean up list
        for (unsigned k = 1; k < stack.size(); k++)
          Remove(i+1);
        // item->m_bIsFolder = true;  // don't treat stacked files as folders
        // the label may be in a different char set from the filename (eg over smb
        // the label is converted from utf8, but the filename is not)
        if (!g_guiSettings.GetBool("filelists.showextensions"))
          CUtil::RemoveExtension(stackName);
        CUtil::URLDecode(stackName);
        item1->SetLabel(stackName);
        item1->m_dwSize = size;
        break;
      }
    }
    i++;
  }
}

bool CFileItemList::Load(int windowID)
{
  CFile file;
  if (file.Open(GetDiscCacheFile(windowID)))
  {
    CLog::Log(LOGDEBUG,"Loading fileitems [%s]",m_strPath.c_str());
    CArchive ar(&file, CArchive::load);
    ar >> *this;
    CLog::Log(LOGDEBUG,"  -- items: %i, directory: %s sort method: %i, ascending: %s",Size(),m_strPath.c_str(), m_sortMethod, m_sortOrder ? "true" : "false");
    ar.Close();
    file.Close();
    return true;
  }

  return false;
}

bool CFileItemList::Save(int windowID)
{
  int iSize = Size();
  if (iSize <= 0)
    return false;

  CLog::Log(LOGDEBUG,"Saving fileitems [%s]",m_strPath.c_str());

  CFile file;
  if (file.OpenForWrite(GetDiscCacheFile(windowID), true)) // overwrite always
  {
    CArchive ar(&file, CArchive::store);
    ar << *this;
    CLog::Log(LOGDEBUG,"  -- items: %i, sort method: %i, ascending: %s",iSize,m_sortMethod, m_sortOrder ? "true" : "false");
    ar.Close();
    file.Close();
    return true;
  }

  return false;
}

void CFileItemList::RemoveDiscCache(int windowID) const
{
  CStdString cacheFile(GetDiscCacheFile(windowID));
  if (CFile::Exists(cacheFile))
  {
    CLog::Log(LOGDEBUG,"Clearing cached fileitems [%s]",m_strPath.c_str());
    CFile::Delete(cacheFile);
  }
}

CStdString CFileItemList::GetDiscCacheFile(int windowID) const
{
  CStdString strPath=m_strPath;
  CUtil::RemoveSlashAtEnd(strPath);

  Crc32 crc;
  crc.ComputeFromLowerCase(strPath);

  CStdString cacheFile;
  if (IsCDDA() || IsOnDVD())
    cacheFile.Format("special://temp/r-%08x.fi", (unsigned __int32)crc);
  else if (IsMusicDb())
    cacheFile.Format("special://temp/mdb-%08x.fi", (unsigned __int32)crc);
  else if (IsVideoDb())
    cacheFile.Format("special://temp/vdb-%08x.fi", (unsigned __int32)crc);
  else if (windowID)
    cacheFile.Format("special://temp/%i-%08x.fi", windowID, (unsigned __int32)crc);
  else
    cacheFile.Format("special://temp/%08x.fi", (unsigned __int32)crc);
  return cacheFile;
}

bool CFileItemList::AlwaysCache() const
{
  // some database folders are always cached
  if (IsMusicDb())
    return CMusicDatabaseDirectory::CanCache(m_strPath);
  if (IsVideoDb())
    return CVideoDatabaseDirectory::CanCache(m_strPath);
  return false;
}

void CFileItemList::SetCachedVideoThumbs()
{
  CSingleLock lock(m_lock);
  // TODO: Investigate caching time to see if it speeds things up
  for (unsigned int i = 0; i < m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->SetCachedVideoThumb();
  }
}

void CFileItemList::SetCachedMusicThumbs()
{
  CSingleLock lock(m_lock);
  // TODO: Investigate caching time to see if it speeds things up
  for (unsigned int i = 0; i < m_items.size(); ++i)
  {
    CFileItemPtr pItem = m_items[i];
    pItem->SetCachedMusicThumb();
  }
}

void CFileItem::SetCachedMusicThumb()
{
  // if it already has a thumbnail, then return
  if (HasThumbnail() || m_bIsShareOrDrive) return ;

  // streams do not have thumbnails
  if (IsInternetStream()) return ;

  //  music db items already have thumbs or there is no thumb available
  if (IsMusicDb()) return;

  // ignore the parent dir items
  if (IsParentFolder()) return;

  CStdString cachedThumb(GetPreviouslyCachedMusicThumb());
  if (!cachedThumb.IsEmpty())
    SetThumbnailImage(cachedThumb);
    // SetIconImage(cachedThumb);
}

CStdString CFileItem::GetPreviouslyCachedMusicThumb() const
{
  // look if an album thumb is available,
  // could be any file with tags loaded or
  // a directory in album window
  CStdString strAlbum, strArtist;
  if (HasMusicInfoTag() && m_musicInfoTag->Loaded())
  {
    strAlbum = m_musicInfoTag->GetAlbum();
    if (!m_musicInfoTag->GetAlbumArtist().IsEmpty())
      strArtist = m_musicInfoTag->GetAlbumArtist();
    else
      strArtist = m_musicInfoTag->GetArtist();
  }
  if (!strAlbum.IsEmpty() && !strArtist.IsEmpty())
  {
    // try permanent album thumb using "album name + artist name"
    CStdString thumb(CUtil::GetCachedAlbumThumb(strAlbum, strArtist));
    if (CFile::Exists(thumb))
      return thumb;
  }

  // if a file, try to find a cached filename.tbn
  if (!m_bIsFolder)
  {
    // look for locally cached tbn
    CStdString thumb(CUtil::GetCachedMusicThumb(m_strPath));
    if (CFile::Exists(thumb))
      return thumb;
  }

  // try and find a cached folder thumb (folder.jpg or folder.tbn)
  CStdString strPath;
  if (!m_bIsFolder)
    CUtil::GetDirectory(m_strPath, strPath);
  else
    strPath = m_strPath;
  // music thumbs are cached without slash at end
  CUtil::RemoveSlashAtEnd(strPath);

  CStdString thumb(CUtil::GetCachedMusicThumb(strPath));
  if (CFile::Exists(thumb))
    return thumb;

  return "";
}

CStdString CFileItem::GetUserMusicThumb(bool alwaysCheckRemote /* = false */) const
{
  if (m_strPath.IsEmpty()
   || m_bIsShareOrDrive
   || IsInternetStream()
   || CUtil::IsUPnP(m_strPath)
   || (CUtil::IsFTP(m_strPath) && !g_advancedSettings.m_bFTPThumbs)
   || IsPlugin()
   || IsAddonsPath()
   || IsParentFolder()
   || IsMusicDb())
    return "";

  // we first check for <filename>.tbn or <foldername>.tbn
  CStdString fileThumb(GetTBNFile());
  if (CFile::Exists(fileThumb))
    return fileThumb;

  // if a folder, check for folder.jpg
  if (m_bIsFolder && !IsFileFolder() && (!IsRemote() || alwaysCheckRemote || g_guiSettings.GetBool("musicfiles.findremotethumbs")))
  {
    CStdStringArray thumbs;
    StringUtils::SplitString(g_advancedSettings.m_musicThumbs, "|", thumbs);
    for (unsigned int i = 0; i < thumbs.size(); ++i)
    {
      CStdString folderThumb(GetFolderThumb(thumbs[i]));
      if (CFile::Exists(folderThumb))
      {
        return folderThumb;
      }
    }
  }
  // this adds support for files which inherit a folder.jpg icon which has not been cached yet.
  // this occurs when queueing a top-level folder which has not been traversed yet.
  else if (!IsRemote() || alwaysCheckRemote || g_guiSettings.GetBool("musicfiles.findremotethumbs"))
  {
    CStdString strFolder, strFile;
    CUtil::Split(m_strPath, strFolder, strFile);
    CFileItem folderItem(strFolder, true);
    folderItem.SetMusicThumb(alwaysCheckRemote);
    if (folderItem.HasThumbnail())
      return folderItem.GetThumbnailImage();
  }
  // No thumb found
  return "";
}

void CFileItem::SetUserMusicThumb(bool alwaysCheckRemote /* = false */)
{
  // caches as the local thumb
  CStdString thumb(GetUserMusicThumb(alwaysCheckRemote));
  if (!thumb.IsEmpty())
  {
    CStdString cachedThumb(CUtil::GetCachedMusicThumb(m_strPath));
    CPicture::CreateThumbnail(thumb, cachedThumb);
  }

  SetCachedMusicThumb();
}

CStdString CFileItem::GetCachedVideoThumb() const
{
  if (IsStack())
    return GetCachedThumb(CStackDirectory::GetFirstStackedFile(m_strPath),g_settings.GetVideoThumbFolder(),true);
  else if (IsVideoDb() && HasVideoInfoTag())
  {
    if (m_bIsFolder && !GetVideoInfoTag()->m_strPath.IsEmpty())
      return GetCachedThumb(GetVideoInfoTag()->m_strPath, g_settings.GetVideoThumbFolder(), true);
    else if (!GetVideoInfoTag()->m_strFileNameAndPath.IsEmpty())
      return GetCachedThumb(GetVideoInfoTag()->m_strFileNameAndPath, g_settings.GetVideoThumbFolder(), true);
  }
  return GetCachedThumb(m_strPath,g_settings.GetVideoThumbFolder(),true);
}

CStdString CFileItem::GetCachedEpisodeThumb() const
{
  // get the locally cached thumb
  CStdString strCRC;
  strCRC.Format("%sepisode%i",GetVideoInfoTag()->m_strFileNameAndPath.c_str(),GetVideoInfoTag()->m_iEpisode);
  return GetCachedThumb(strCRC,g_settings.GetVideoThumbFolder(),true);
}

void CFileItem::SetCachedVideoThumb()
{
  if (IsParentFolder()) return;
  if (HasThumbnail()) return;
  CStdString cachedThumb(GetCachedVideoThumb());
  if (HasVideoInfoTag() && !m_bIsFolder  &&
      GetVideoInfoTag()->m_iEpisode > -1 &&
      CFile::Exists(GetCachedEpisodeThumb()))
  {
    SetThumbnailImage(GetCachedEpisodeThumb());
  }
  else if (CFile::Exists(cachedThumb))
    SetThumbnailImage(cachedThumb);
}

// Gets the .tbn filename from a file or folder name.
// <filename>.ext -> <filename>.tbn
// <foldername>/ -> <foldername>.tbn
CStdString CFileItem::GetTBNFile() const
{
  CStdString thumbFile;
  CStdString strFile = m_strPath;

  if (IsStack())
  {
    CStdString strPath, strReturn;
    CUtil::GetParentPath(m_strPath,strPath);
    CFileItem item(CStackDirectory::GetFirstStackedFile(strFile),false);
    CStdString strTBNFile = item.GetTBNFile();
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strTBNFile),strReturn);
    if (CFile::Exists(strReturn))
      return strReturn;

    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(CStackDirectory::GetStackedTitlePath(strFile)),strFile);
  }

  if (CUtil::IsInRAR(strFile) || CUtil::IsInZIP(strFile))
  {
    CStdString strPath, strParent;
    CUtil::GetDirectory(strFile,strPath);
    CUtil::GetParentPath(strPath,strParent);
    CUtil::AddFileToFolder(strParent,CUtil::GetFileName(m_strPath),strFile);
  }

  CURL url(strFile);
  strFile = url.GetFileName();

  if (m_bIsFolder && !IsFileFolder())
    CUtil::RemoveSlashAtEnd(strFile);

  if (!strFile.IsEmpty())
  {
    if (m_bIsFolder && !IsFileFolder())
      thumbFile = strFile + ".tbn"; // folder, so just add ".tbn"
    else
      thumbFile = CUtil::ReplaceExtension(strFile, ".tbn");
    url.SetFileName(thumbFile);
    thumbFile = url.Get();
  }
  return thumbFile;
}

CStdString CFileItem::GetUserVideoThumb() const
{
  if (IsTuxBox())
  {
    if (!m_bIsFolder)
      return g_tuxbox.GetPicon(GetLabel());
    else return "";
  }

  if (m_strPath.IsEmpty()
   || m_bIsShareOrDrive
   || IsInternetStream()
   || CUtil::IsUPnP(m_strPath)
   || (CUtil::IsFTP(m_strPath) && !g_advancedSettings.m_bFTPThumbs)
   || IsPlugin()
   || IsAddonsPath()
   || IsParentFolder()
   || IsLiveTV())
    return "";


  // 1. check <filename>.tbn or <foldername>.tbn
  CStdString fileThumb(GetTBNFile());
  if (CFile::Exists(fileThumb))
    return fileThumb;

  // 2. - check movie.tbn, as long as it's not a folder
  if (!m_bIsFolder)
  {
    CStdString strPath, movietbnFile;
    CUtil::GetParentPath(m_strPath, strPath);
    CUtil::AddFileToFolder(strPath, "movie.tbn", movietbnFile);
    if (CFile::Exists(movietbnFile))
      return movietbnFile;
  }

  // 3. check folder image in_m_dvdThumbs (folder.jpg)
  if (m_bIsFolder && !IsFileFolder())
  {
    CStdStringArray thumbs;
    StringUtils::SplitString(g_advancedSettings.m_dvdThumbs, "|", thumbs);
    for (unsigned int i = 0; i < thumbs.size(); ++i)
    {
      CStdString folderThumb(GetFolderThumb(thumbs[i]));
      if (CFile::Exists(folderThumb))
      {
        return folderThumb;
      }
    }
  }
  // No thumb found
  return "";
}

CStdString CFileItem::GetFolderThumb(const CStdString &folderJPG /* = "folder.jpg" */) const
{
  CStdString folderThumb;
  CStdString strFolder = m_strPath;

  if (IsStack() ||
      CUtil::IsInRAR(strFolder) ||
      CUtil::IsInZIP(strFolder))
  {
    CUtil::GetParentPath(m_strPath,strFolder);
  }

  if (IsMultiPath())
    strFolder = CMultiPathDirectory::GetFirstPath(m_strPath);

  CUtil::AddFileToFolder(strFolder, folderJPG, folderThumb);
  return folderThumb;
}

CStdString CFileItem::GetMovieName(bool bUseFolderNames /* = false */) const
{
  if (IsLabelPreformated())
    return GetLabel();

  CStdString strMovieName = m_strPath;

  if (IsMultiPath())
    strMovieName = CMultiPathDirectory::GetFirstPath(m_strPath);

  if (CUtil::IsStack(strMovieName))
    strMovieName = CStackDirectory::GetStackedTitlePath(strMovieName);

  int pos;
  if ((pos=strMovieName.Find("BDMV/")) != -1 ||
      (pos=strMovieName.Find("BDMV\\")) != -1)
    strMovieName = strMovieName.Mid(0,pos+5);

  if ((!m_bIsFolder || IsDVDFile(false, true) || CUtil::IsInArchive(m_strPath)) && bUseFolderNames)
  {
    CStdString name2(strMovieName);
    CUtil::GetParentPath(name2,strMovieName);
    if (CUtil::IsInArchive(m_strPath) || strMovieName.Find( "VIDEO_TS" ) != -1)
    {
      CStdString strArchivePath;
      CUtil::GetParentPath(strMovieName, strArchivePath);
      strMovieName = strArchivePath;
    }
  }

  CUtil::RemoveSlashAtEnd(strMovieName);
  strMovieName = CUtil::GetFileName(strMovieName);
  CUtil::URLDecode(strMovieName);

  return strMovieName;
}

void CFileItem::SetVideoThumb()
{
  if (HasThumbnail()) return;
  SetCachedVideoThumb();
  if (!HasThumbnail())
    SetUserVideoThumb();
}

void CFileItem::SetUserVideoThumb()
{
  if (m_bIsShareOrDrive) return;
  if (IsParentFolder()) return;

  // caches as the local thumb
  CStdString thumb(GetUserVideoThumb());
  if (!thumb.IsEmpty())
  {
    CStdString cachedThumb(GetCachedVideoThumb());
    CPicture::CreateThumbnail(thumb, cachedThumb);
  }
  SetCachedVideoThumb();
}

bool CFileItem::CacheLocalFanart() const
{
  // first check for an already cached fanart image
  CStdString cachedFanart(GetCachedFanart());
  if (CFile::Exists(cachedFanart))
    return true;

  // we don't have a cached image, so let's see if the user has a local image, and cache it if so
  CStdString localFanart(GetLocalFanart());
  if (!localFanart.IsEmpty())
    return CPicture::CacheFanart(localFanart, cachedFanart);
  return false;
}

CStdString CFileItem::GetLocalFanart() const
{
  if (IsVideoDb())
  {
    if (!HasVideoInfoTag())
      return ""; // nothing can be done
    CFileItem dbItem(m_bIsFolder ? GetVideoInfoTag()->m_strPath : GetVideoInfoTag()->m_strFileNameAndPath, m_bIsFolder);
    return dbItem.GetLocalFanart();
  }

  CStdString strFile2;
  CStdString strFile = m_strPath;
  if (IsStack())
  {
    CStdString strPath;
    CUtil::GetParentPath(m_strPath,strPath);
    CStackDirectory dir;
    CStdString strPath2;
    strPath2 = dir.GetStackedTitlePath(strFile);
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strPath2),strFile);
    CFileItem item(dir.GetFirstStackedFile(m_strPath),false);
    CStdString strTBNFile(CUtil::ReplaceExtension(item.GetTBNFile(), "-fanart"));
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strTBNFile),strFile2);
  }
  if (CUtil::IsInRAR(strFile) || CUtil::IsInZIP(strFile))
  {
    CStdString strPath, strParent;
    CUtil::GetDirectory(strFile,strPath);
    CUtil::GetParentPath(strPath,strParent);
    CUtil::AddFileToFolder(strParent,CUtil::GetFileName(m_strPath),strFile);
  }

  // no local fanart available for these
  if (IsInternetStream()
   || CUtil::IsUPnP(strFile)
   || IsLiveTV()
   || IsPlugin()
   || IsAddonsPath()
   || (CUtil::IsFTP(strFile) && !g_advancedSettings.m_bFTPThumbs)
   || m_strPath.IsEmpty())
    return "";

  CStdString strDir;
  CUtil::GetDirectory(strFile, strDir);

  if (strDir.IsEmpty())
    return "";

  CFileItemList items;
  CDirectory::GetDirectory(strDir, items, g_settings.m_pictureExtensions, false, false, DIR_CACHE_ALWAYS, false, true);

  CStdStringArray fanarts;
  StringUtils::SplitString(g_advancedSettings.m_fanartImages, "|", fanarts);

  strFile = CUtil::ReplaceExtension(strFile, "-fanart");
  fanarts.insert(m_bIsFolder ? fanarts.end() : fanarts.begin(), CUtil::GetFileName(strFile));

  if (!strFile2.IsEmpty())
    fanarts.insert(m_bIsFolder ? fanarts.end() : fanarts.begin(), CUtil::GetFileName(strFile2));

  for (unsigned int i = 0; i < fanarts.size(); ++i)
  {
    for (int j = 0; j < items.Size(); j++)
    {
      CStdString strCandidate = CUtil::GetFileName(items[j]->m_strPath);
      CUtil::RemoveExtension(strCandidate);
      CStdString strFanart = fanarts[i];
      CUtil::RemoveExtension(strFanart);
      if (strCandidate.CompareNoCase(strFanart) == 0)
        return items[j]->m_strPath;
    }
  }

  return "";
}

CStdString CFileItem::GetCachedFanart() const
{
  // get the locally cached thumb
  if (IsVideoDb())
  {
    if (!HasVideoInfoTag())
      return "";
    if (!GetVideoInfoTag()->m_strArtist.IsEmpty())
      return GetCachedThumb(GetVideoInfoTag()->m_strArtist,g_settings.GetMusicFanartFolder());
    if (!m_bIsFolder && !GetVideoInfoTag()->m_strShowTitle.IsEmpty())
    {
      CVideoDatabase database;
      database.Open();
      int iShowId = database.GetTvShowId(GetVideoInfoTag()->m_strPath);
      CStdString showPath;
      database.GetFilePathById(iShowId,showPath,VIDEODB_CONTENT_TVSHOWS);
      return GetCachedThumb(showPath,g_settings.GetVideoFanartFolder());
    }
    return GetCachedThumb(m_bIsFolder ? GetVideoInfoTag()->m_strPath : GetVideoInfoTag()->m_strFileNameAndPath,g_settings.GetVideoFanartFolder());
  }
  if (HasMusicInfoTag())
    return GetCachedThumb(GetMusicInfoTag()->GetArtist(),g_settings.GetMusicFanartFolder());

  return GetCachedThumb(m_strPath,g_settings.GetVideoFanartFolder());
}

CStdString CFileItem::GetCachedThumb(const CStdString &path, const CStdString &path2, bool split)
{
  // get the locally cached thumb
  Crc32 crc;
  crc.ComputeFromLowerCase(path);

  CStdString thumb;
  if (split)
  {
    CStdString hex;
    hex.Format("%08x", (__int32)crc);
    thumb.Format("%c\\%08x.tbn", hex[0], (unsigned __int32)crc);
  }
  else
    thumb.Format("%08x.tbn", (unsigned __int32)crc);

  return CUtil::AddFileToFolder(path2, thumb);
}

/*void CFileItem::SetThumb()
{
  // we need to know the type of file at this point
  // as differing views have differing inheritance rules for thumbs:

  // Videos:
  // Folders only use <foldername>/folder.jpg or <foldername>.tbn
  // Files use <filename>.tbn
  //  * Thumbs are cached from here using file or folder path

  // Music:
  // Folders only use <foldername>/folder.jpg or <foldername>.tbn
  // Files use <filename>.tbn or the album/path cached thumb or inherit from the folder
  //  * Thumbs are cached from here using file or folder path

  // Programs:
  // Folders only use <foldername>/folder.jpg or <foldername>.tbn
  // Files use <filename>.tbn or the embedded xbe.  Shortcuts have the additional step of the <thumbnail> tag to check
  //  * Thumbs are cached from here using file or folder path

  // Pictures:
  // Folders use <foldername>/folder.jpg or <foldername>.tbn, or auto-generated from 4 images in the folder
  // Files use <filename>.tbn or a resized version of the picture
  //  * Thumbs are cached from here using file or folder path

}*/

bool CFileItem::LoadMusicTag()
{
  // not audio
  if (!IsAudio())
    return false;
  // already loaded?
  if (HasMusicInfoTag() && m_musicInfoTag->Loaded())
    return true;
  // check db
  CMusicDatabase musicDatabase;
  if (musicDatabase.Open())
  {
    CSong song;
    if (musicDatabase.GetSongByFileName(m_strPath, song))
    {
      GetMusicInfoTag()->SetSong(song);
      SetThumbnailImage(song.strThumb);
      return true;
    }
    musicDatabase.Close();
  }
  // load tag from file
  CLog::Log(LOGDEBUG, "%s: loading tag information for file: %s", __FUNCTION__, m_strPath.c_str());
  CMusicInfoTagLoaderFactory factory;
  auto_ptr<IMusicInfoTagLoader> pLoader (factory.CreateLoader(m_strPath));
  if (NULL != pLoader.get())
  {
    if (pLoader->Load(m_strPath, *GetMusicInfoTag()))
      return true;
  }
  // no tag - try some other things
  if (IsCDDA())
  {
    // we have the tracknumber...
    int iTrack = GetMusicInfoTag()->GetTrackNumber();
    if (iTrack >= 1)
    {
      CStdString strText = g_localizeStrings.Get(554); // "Track"
      if (strText.GetAt(strText.size() - 1) != ' ')
        strText += " ";
      CStdString strTrack;
      strTrack.Format(strText + "%i", iTrack);
      GetMusicInfoTag()->SetTitle(strTrack);
      GetMusicInfoTag()->SetLoaded(true);
      return true;
    }
  }
  else
  {
    CStdString fileName = CUtil::GetFileName(m_strPath);
    CUtil::RemoveExtension(fileName);
    for (unsigned int i = 0; i < g_advancedSettings.m_musicTagsFromFileFilters.size(); i++)
    {
      CLabelFormatter formatter(g_advancedSettings.m_musicTagsFromFileFilters[i], "");
      if (formatter.FillMusicTag(fileName, GetMusicInfoTag()))
      {
        GetMusicInfoTag()->SetLoaded(true);
        return true;
      }
    }
  }
  return false;
}

void CFileItemList::Swap(unsigned int item1, unsigned int item2)
{
  if (item1 != item2 && item1 < m_items.size() && item2 < m_items.size())
    std::swap(m_items[item1], m_items[item2]);
}

bool CFileItemList::UpdateItem(const CFileItem *item)
{
  if (!item) return false;
  CFileItemPtr oldItem = Get(item->m_strPath);
  if (oldItem)
    *oldItem = *item;
  return oldItem;
}

void CFileItemList::AddSortMethod(SORT_METHOD sortMethod, int buttonLabel, const LABEL_MASKS &labelMasks)
{
  SORT_METHOD_DETAILS sort;
  sort.m_sortMethod=sortMethod;
  sort.m_buttonLabel=buttonLabel;
  sort.m_labelMasks=labelMasks;

  m_sortDetails.push_back(sort);
}

void CFileItemList::SetReplaceListing(bool replace)
{
  m_replaceListing = replace;
}

void CFileItemList::ClearSortState()
{
  m_sortMethod=SORT_METHOD_NONE;
  m_sortOrder=SORT_ORDER_NONE;
}

CVideoInfoTag* CFileItem::GetVideoInfoTag()
{
  if (!m_videoInfoTag)
    m_videoInfoTag = new CVideoInfoTag;

  return m_videoInfoTag;
}

CPictureInfoTag* CFileItem::GetPictureInfoTag()
{
  if (!m_pictureInfoTag)
    m_pictureInfoTag = new CPictureInfoTag;

  return m_pictureInfoTag;
}

MUSIC_INFO::CMusicInfoTag* CFileItem::GetMusicInfoTag()
{
  if (!m_musicInfoTag)
    m_musicInfoTag = new MUSIC_INFO::CMusicInfoTag;

  return m_musicInfoTag;
}

CStdString CFileItem::FindTrailer() const
{
  CStdString strFile2, strTrailer;
  CStdString strFile = m_strPath;
  if (IsStack())
  {
    CStdString strPath;
    CUtil::GetParentPath(m_strPath,strPath);
    CStackDirectory dir;
    CStdString strPath2;
    strPath2 = dir.GetStackedTitlePath(strFile);
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strPath2),strFile);
    CFileItem item(dir.GetFirstStackedFile(m_strPath),false);
    CStdString strTBNFile(CUtil::ReplaceExtension(item.GetTBNFile(), "-trailer"));
    CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strTBNFile),strFile2);
  }
  if (CUtil::IsInRAR(strFile) || CUtil::IsInZIP(strFile))
  {
    CStdString strPath, strParent;
    CUtil::GetDirectory(strFile,strPath);
    CUtil::GetParentPath(strPath,strParent);
    CUtil::AddFileToFolder(strParent,CUtil::GetFileName(m_strPath),strFile);
  }

  // no local trailer available for these
  if (IsInternetStream()
   || CUtil::IsUPnP(strFile)
   || IsLiveTV()
   || IsPlugin())
    return strTrailer;

  CStdString strDir;
  CUtil::GetDirectory(strFile, strDir);
  CFileItemList items;
  CDirectory::GetDirectory(strDir, items, g_settings.m_videoExtensions, true, false, DIR_CACHE_ALWAYS, false, true);
  CUtil::RemoveExtension(strFile);
  strFile += "-trailer";
  CStdString strFile3 = CUtil::AddFileToFolder(strDir, "movie-trailer");

  // Precompile our REs
  VECCREGEXP matchRegExps;
  CRegExp tmpRegExp(true);
  const CStdStringArray& strMatchRegExps = g_advancedSettings.m_trailerMatchRegExps;

  CStdStringArray::const_iterator strRegExp = strMatchRegExps.begin();
  while (strRegExp != strMatchRegExps.end())
  {
    if (tmpRegExp.RegComp(*strRegExp))
    {
      matchRegExps.push_back(tmpRegExp);
    }
    strRegExp++;
  }

  for (int i = 0; i < items.Size(); i++)
  {
    CStdString strCandidate = items[i]->m_strPath;
    CUtil::RemoveExtension(strCandidate);
    if (strCandidate.CompareNoCase(strFile) == 0 ||
        strCandidate.CompareNoCase(strFile2) == 0 ||
        strCandidate.CompareNoCase(strFile3) == 0)
    {
      strTrailer = items[i]->m_strPath;
      break;
    }
    else
    {
      VECCREGEXP::iterator expr = matchRegExps.begin();

      while (expr != matchRegExps.end())
      {
        if (expr->RegFind(strCandidate) != -1)
        {
          strTrailer = items[i]->m_strPath;
          i = items.Size();
          break;
        }
        expr++;
      }
    }
  }

  return strTrailer;
}



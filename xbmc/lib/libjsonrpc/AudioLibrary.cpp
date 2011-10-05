/*
 *      Copyright (C) 2005-2010 Team XBMC
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

#include "AudioLibrary.h"
#include "../MusicDatabase.h"
#include "../FileItem.h"
#include "../Util.h"
#include "../MusicInfoTag.h"
#include "../Song.h"
#include "Application.h"

using namespace MUSIC_INFO;
using namespace Json;
using namespace JSONRPC;

JSON_STATUS CAudioLibrary::GetArtists(const CStdString &method, ITransportLayer *transport, IClient *client, const Value &parameterObject, Value &result)
{
  if (!(parameterObject.isObject() || parameterObject.isNull()))
    return InvalidParams;

  const Value param = ForceObject(parameterObject);
  if (!ParameterIntOrNull(param, "genreid"))
    return InvalidParams;

  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  int genreID = ParameterAsInt(param, -1, "genreid");

  CFileItemList items;
  if (musicdatabase.GetArtistsNav("", items, genreID, false))
    HandleFileItemList("artistid", false, "artists", items, param, result);

  musicdatabase.Close();
  return OK;
}

JSON_STATUS CAudioLibrary::GetAlbums(const CStdString &method, ITransportLayer *transport, IClient *client, const Value &parameterObject, Value &result)
{
  if (!(parameterObject.isObject() || parameterObject.isNull()))
    return InvalidParams;

  const Value param = ForceObject(parameterObject);
  if (!(ParameterIntOrNull(param, "artistid") || ParameterIntOrNull(param, "albumid")))
    return InvalidParams;

  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  int artistID = ParameterAsInt(param, -1, "artistid");
  int genreID  = ParameterAsInt(param, -1, "genreid");
  int start = ParameterAsInt(param, -1, "start");
  int end = ParameterAsInt(param, -1, "end");

  CFileItemList items;
  if (musicdatabase.GetAlbumsNav("", items, genreID, artistID, start, end))
    HandleFileItemList("albumid", false, "albums", items, param, result);

  musicdatabase.Close();
  return OK;
}

JSON_STATUS CAudioLibrary::GetSongs(const CStdString &method, ITransportLayer *transport, IClient *client, const Value &parameterObject, Value &result)
{
  if (!(parameterObject.isObject() || parameterObject.isNull()))
    return InvalidParams;

  const Value param = ForceObject(parameterObject);
  if (!(ParameterIntOrNull(param, "artistid") || ParameterIntOrNull(param, "albumid") || ParameterIntOrNull(param, "genreid")))
    return InvalidParams;

  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  int artistID = ParameterAsInt(param, -1, "artistid");
  int albumID  = ParameterAsInt(param, -1, "albumid");
  int genreID  = ParameterAsInt(param, -1, "genreid");

  CFileItemList items;
  if (musicdatabase.GetSongsNav("", items, genreID, artistID, albumID))
    HandleFileItemList("songid", true, "songs", items, param, result);

  musicdatabase.Close();
  return OK;
}

JSON_STATUS CAudioLibrary::ScanForContent(const CStdString &method, ITransportLayer *transport, IClient *client, const Value &parameterObject, Value &result)
{
  g_application.getApplicationMessenger().ExecBuiltIn("updatelibrary(music)");
  return ACK;
}

bool CAudioLibrary::FillFileItemList(const Value &parameterObject, CFileItemList &list)
{
  CMusicDatabase musicdatabase;
  bool success = false;

  if (musicdatabase.Open())
  {
    if (parameterObject["artistid"].isInt() || parameterObject["albumid"].isInt() || parameterObject["genreid"].isInt())
    {
      int artistID = ParameterAsInt(parameterObject, -1, "artistid");
      int albumID  = ParameterAsInt(parameterObject, -1, "albumid");
      int genreID  = ParameterAsInt(parameterObject, -1, "genreid");

      success = musicdatabase.GetSongsNav("", list, genreID, artistID, albumID);
    }
    if (parameterObject["songid"].isInt())
    {
      int songID = ParameterAsInt(parameterObject, -1, "songid");
      if (songID != -1)
      {
        CSong song;
        if (musicdatabase.GetSongById(songID, song))
        {
          list.Add(CFileItemPtr(new CFileItem(song)));
          success = true;
        }
      }
    }

    musicdatabase.Close();
  }

  return success;
}

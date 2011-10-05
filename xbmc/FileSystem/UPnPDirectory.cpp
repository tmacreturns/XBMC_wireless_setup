/*
* UPnP Support for XBMC
* Copyright (c) 2006 c0diq (Sylvain Rebaud)
* Portions Copyright (c) by the authors of libPlatinum
*
* http://www.plutinosoft.com/blog/category/platinum/
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "Util.h"
#include "UPnPDirectory.h"
#include "UPnP.h"
#include "Platinum.h"
#include "PltSyncMediaBrowser.h"
#include "VideoInfoTag.h"
#include "MusicInfoTag.h"
#include "FileItem.h"
#include "GUIWindowManager.h"
#include "GUIDialogProgress.h"
#include "utils/log.h"

using namespace MUSIC_INFO;
using namespace XFILE;

namespace XFILE
{
/*----------------------------------------------------------------------
|   CProtocolFinder
+---------------------------------------------------------------------*/
class CProtocolFinder {
public:
    CProtocolFinder(const char* protocol) : m_Protocol(protocol) {}
    bool operator()(const PLT_MediaItemResource& resource) const {
        return (resource.m_ProtocolInfo.ToString().Compare(m_Protocol, true) == 0);
    }
private:
    NPT_String m_Protocol;
};

/*----------------------------------------------------------------------
|   CUPnPDirectory::GetFriendlyName
+---------------------------------------------------------------------*/
const char*
CUPnPDirectory::GetFriendlyName(const char* url)
{
    NPT_String path = url;
    if (!path.EndsWith("/")) path += "/";

    if (path.Left(7).Compare("upnp://", true) != 0) {
        return NULL;
    } else if (path.Compare("upnp://", true) == 0) {
        return "UPnP Media Servers (Auto-Discover)";
    }

    // look for nextslash
    int next_slash = path.Find('/', 7);
    if (next_slash == -1)
        return NULL;

    NPT_String uuid = path.SubString(7, next_slash-7);
    NPT_String object_id = path.SubString(next_slash+1, path.GetLength()-next_slash-2);

    if (!CUPnP::GetInstance()->IsClientStarted())
        return NULL;

    // look for device
    PLT_DeviceDataReference device;
    if (NPT_FAILED(CUPnP::GetInstance()->m_MediaBrowser->FindServer(uuid, device)) || device.IsNull())
        return NULL;

    return (const char*)device->GetFriendlyName();
}

/*----------------------------------------------------------------------
|   CUPnPDirectory::GetDirectory
+---------------------------------------------------------------------*/
bool
CUPnPDirectory::GetDirectory(const CStdString& strPath, CFileItemList &items)
{
    CUPnP* upnp = CUPnP::GetInstance();

    /* upnp should never be cached, it has internal cache */
    items.SetCacheToDisc(CFileItemList::CACHE_NEVER);

    // start client if it hasn't been done yet
    bool client_started = upnp->IsClientStarted();
    upnp->StartClient();

    // We accept upnp://devuuid/[item_id/]
    NPT_String path = strPath.c_str();
    if (!path.StartsWith("upnp://", true)) {
        return false;
    }

    if (path.Compare("upnp://", true) == 0) {
        // root -> get list of devices
        const NPT_Lock<PLT_DeviceDataReferenceList>& devices = upnp->m_MediaBrowser->GetMediaServers();
        NPT_List<PLT_DeviceDataReference>::Iterator device = devices.GetFirstItem();
        while (device) {
            NPT_String name = (*device)->GetFriendlyName();
            NPT_String uuid = (*device)->GetUUID();

            CFileItemPtr pItem(new CFileItem((const char*)name));
            pItem->m_strPath = (const char*) "upnp://" + uuid + "/";
            pItem->m_bIsFolder = true;
            pItem->SetThumbnailImage((const char*)(*device)->GetIconUrl("image/jpeg"));

            items.Add(pItem);

            ++device;
        }
    } else {
        if (!path.EndsWith("/")) path += "/";

        // look for nextslash
        int next_slash = path.Find('/', 7);

        NPT_String uuid = (next_slash==-1)?path.SubString(7):path.SubString(7, next_slash-7);
        NPT_String object_id = (next_slash==-1)?"":path.SubString(next_slash+1);
        object_id.TrimRight("/");
        if (object_id.GetLength()) {
            CStdString tmp = (char*) object_id;
            CUtil::URLDecode(tmp);
            object_id = tmp;
        }

        // look for device in our list
        // (and wait for it to respond for 5 secs if we're just starting upnp client)
        NPT_TimeStamp watchdog;
        NPT_System::GetCurrentTimeStamp(watchdog);
        watchdog += 5.f;

        PLT_DeviceDataReference device;
        for (;;) {
            if (NPT_SUCCEEDED(upnp->m_MediaBrowser->FindServer(uuid, device)) && !device.IsNull())
                break;

            // fail right away if device not found and upnp client was already running
            if (client_started)
                goto failure;

            // otherwise check if we've waited long enough without success
            NPT_TimeStamp now;
            NPT_System::GetCurrentTimeStamp(now);
            if (now > watchdog)
                goto failure;

            // sleep a bit and try again
            NPT_System::Sleep(NPT_TimeInterval(1, 0));
        }

        // issue a browse request with object_id
        // if object_id is empty use "0" for root
        object_id = object_id.IsEmpty()?"0":object_id;

        // just a guess as to what types of files we want
        bool video = true;
        bool audio = true;
        bool image = true;
        m_strFileMask.TrimLeft("/");
        if (!m_strFileMask.IsEmpty()) {
            video = m_strFileMask.Find(".wmv") >= 0;
            audio = m_strFileMask.Find(".wma") >= 0;
            image = m_strFileMask.Find(".jpg") >= 0;
        }

        // special case for Windows Media Connect and WMP11 when looking for root
        // We can target which root subfolder we want based on directory mask
        if (object_id == "0" && ((device->GetFriendlyName().Find("Windows Media Connect", 0, true) >= 0) ||
                                 (device->m_ModelName == "Windows Media Player Sharing"))) {

            // look for a specific type to differentiate which folder we want
            if (audio && !video && !image) {
                // music
                object_id = "1";
            } else if (!audio && video && !image) {
                // video
                object_id = "2";
            } else if (!audio && !video && image) {
                // pictures
                object_id = "3";
            }
        }

#ifdef DISABLE_SPECIALCASE
        // same thing but special case for XBMC
        if (object_id == "0" && ((device->m_ModelName.Find("XBMC", 0, true) >= 0) ||
                                 (device->m_ModelName.Find("Xbox Media Center", 0, true) >= 0))) {
            // look for a specific type to differentiate which folder we want
            if (audio && !video && !image) {
                // music
                object_id = "virtualpath://upnpmusic";
            } else if (!audio && video && !image) {
                // video
                object_id = "virtualpath://upnpvideo";
            } else if (!audio && !video && image) {
                // pictures
                object_id = "virtualpath://upnppictures";
            }
        }
#endif

        // if error, return now, the device could have gone away
        // this will make us go back to the sources list
        PLT_MediaObjectListReference list;
        NPT_Result res = upnp->m_MediaBrowser->BrowseSync(device, object_id, list);
        if (NPT_FAILED(res)) goto failure;

        // empty list is ok
        if (list.IsNull()) goto cleanup;

        PLT_MediaObjectList::Iterator entry = list->GetFirstItem();
        while (entry) {
            // disregard items with wrong class/type
            if( (!video && (*entry)->m_ObjectClass.type.CompareN("object.item.videoitem", 21,true) == 0)
             || (!audio && (*entry)->m_ObjectClass.type.CompareN("object.item.audioitem", 21,true) == 0)
             || (!image && (*entry)->m_ObjectClass.type.CompareN("object.item.imageitem", 21,true) == 0) )
            {
                ++entry;
                continue;
            }

            // never show empty containers in media views
            if((*entry)->IsContainer()) {
                if( (audio || video || image)
                 && ((PLT_MediaContainer*)(*entry))->m_ChildrenCount == 0) {
                    ++entry;
                    continue;
                }
            }

            CFileItemPtr pItem(new CFileItem((const char*)(*entry)->m_Title));
            pItem->SetLabelPreformated(true);
            pItem->m_strTitle = (const char*)(*entry)->m_Title;
            pItem->m_bIsFolder = (*entry)->IsContainer();

            // if it's a container, format a string as upnp://uuid/object_id
            if (pItem->m_bIsFolder) {
                CStdString id = (char*) (*entry)->m_ObjectID;
                CUtil::URLEncode(id);
                pItem->m_strPath = (const char*) "upnp://" + uuid + "/" + id.c_str() + "/";
            } else {
                if ((*entry)->m_Resources.GetItemCount()) {
                    PLT_MediaItemResource& resource = (*entry)->m_Resources[0];

                    // look for a resource with "xbmc-get" protocol
                    // if we can't find one, keep the first resource
                    NPT_ContainerFind((*entry)->m_Resources,
                                      CProtocolFinder("xbmc-get"),
                                      resource);

                    CLog::Log(LOGDEBUG, "CUPnPDirectory::GetDirectory - resource protocol info '%s'",
                        (const char*)(resource.m_ProtocolInfo.ToString()));

                    // if it's an item, path is the first url to the item
                    // we hope the server made the first one reachable for us
                    // (it could be a format we dont know how to play however)
                    pItem->m_strPath = (const char*) resource.m_Uri;

                    // set metadata
                    if (resource.m_Size != (NPT_LargeSize)-1) {
                        pItem->m_dwSize  = resource.m_Size;
                    }

                    // set a general content type
                    CStdString type = (const char*)(*entry)->m_ObjectClass.type.Left(21);
                    if (type.Equals("object.item.videoitem"))
                        pItem->SetMimeType("video/octet-stream");
                    else if(type.Equals("object.item.audioitem"))
                        pItem->SetMimeType("audio/octet-stream");
                    else if(type.Equals("object.item.imageitem"))
                        pItem->SetMimeType("image/octet-stream");

                    // look for content type in protocol info
                    if (resource.m_ProtocolInfo.IsValid()) {
                        if (resource.m_ProtocolInfo.GetContentType().Compare("application/octet-stream") != 0) {
                            pItem->SetMimeType((const char*)resource.m_ProtocolInfo.GetContentType());
                        }
                    } else {
                        CLog::Log(LOGERROR, "CUPnPDirectory::GetDirectory - invalid protocol info '%s'",
                            (const char*)(resource.m_ProtocolInfo.ToString()));
                    }

                    // look for date?
                    if((*entry)->m_Description.date.GetLength()) {
                        SYSTEMTIME time = {};
                        sscanf((*entry)->m_Description.date, "%hu-%hu-%huT%hu:%hu:%hu",
                               &time.wYear, &time.wMonth, &time.wDay, &time.wHour, &time.wMinute, &time.wSecond);
                        pItem->m_dateTime = time;
                    }

                    // look for metadata
                    if( (*entry)->m_ObjectClass.type.CompareN("object.item.videoitem", 21,true) == 0 ) {
                        pItem->SetLabelPreformated(false);
                        CUPnP::PopulateTagFromObject(*pItem->GetVideoInfoTag(), *(*entry), &resource);
                    } else if( (*entry)->m_ObjectClass.type.CompareN("object.item.audioitem", 21,true) == 0 ) {
                        pItem->SetLabelPreformated(false);
                        CUPnP::PopulateTagFromObject(*pItem->GetMusicInfoTag(), *(*entry), &resource);
                    } else if( (*entry)->m_ObjectClass.type.CompareN("object.item.imageitem", 21,true) == 0 ) {
                      //CPictureInfoTag* tag = pItem->GetPictureInfoTag();
                    }
                }

                // look for subtitles
                unsigned subs = 0;
                for(unsigned r = 0; r < (*entry)->m_Resources.GetItemCount(); r++)
                {
                    PLT_MediaItemResource& res  = (*entry)->m_Resources[r];
                    PLT_ProtocolInfo&      info = res.m_ProtocolInfo;
                    static const char* allowed[] = { "text/srt"
                                                   , "text/ssa"
                                                   , "text/sub"
                                                   , "text/idx" };
                    for(unsigned type = 0; type < sizeof(allowed)/sizeof(allowed[0]); type++)
                    {
                        if(info.Match(PLT_ProtocolInfo("*", "*", allowed[type], "*")))
                        {
                            CStdString prop;
                            prop.Format("upnp:subtitle:%d", ++subs);
                            pItem->SetProperty(prop, (const char*)res.m_Uri);
                            break;
                        }
                    }
                }
            }

            // if there is a thumbnail available set it here
            if((*entry)->m_ExtraInfo.album_art_uri.GetLength())
                pItem->SetThumbnailImage((const char*) (*entry)->m_ExtraInfo.album_art_uri);
            else if((*entry)->m_Description.icon_uri.GetLength())
                pItem->SetThumbnailImage((const char*) (*entry)->m_Description.icon_uri);

            items.Add(pItem);

            ++entry;
        }
    }

cleanup:
    return true;

failure:
    return false;
}
}

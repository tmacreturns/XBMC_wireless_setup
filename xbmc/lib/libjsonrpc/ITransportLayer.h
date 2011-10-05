#pragma once
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

#include <string>
#include "IClient.h"

namespace Json
{
  class Value;
}

namespace JSONRPC
{
  enum TransportLayerCapability
  {
    Response = 0x1,
    Announcing = 0x2,
    FileDownload = 0x4,
  };

  #define TRANSPORT_LAYER_CAPABILITY_ALL (Response | Announcing | FileDownload)

  class ITransportLayer
  {
  public:
    virtual ~ITransportLayer() { };
    virtual bool Download(const char *path, Json::Value *result) = 0;
    virtual int GetCapabilities() = 0;
  };
}

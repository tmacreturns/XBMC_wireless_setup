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

#ifndef WIN_SYSTEM_WIN32_DX_H
#define WIN_SYSTEM_WIN32_DX_H

#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <dxdiag.h>
#include "WinSystemWin32.h"
#include "RenderSystemDX.h"

#ifdef HAS_DX

class CWinSystemWin32DX : public CWinSystemWin32, public CRenderSystemDX
{
public:
  CWinSystemWin32DX();
  ~CWinSystemWin32DX();

  virtual bool CreateNewWindow(CStdString name, bool fullScreen, RESOLUTION_INFO& res, PHANDLE_EVENT_FUNC userFunction);
  virtual bool ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop);
  virtual bool SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays);

protected:
  virtual void UpdateMonitor();
  bool UseWindowedDX(bool fullScreen);
};

#endif

#endif // WIN_SYSTEM_WIN32_DX_H

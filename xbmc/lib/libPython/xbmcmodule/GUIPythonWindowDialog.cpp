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

#include "GUIPythonWindowDialog.h"
#include "GUIWindowManager.h"
#include "Application.h"

CGUIPythonWindowDialog::CGUIPythonWindowDialog(int id)
:CGUIPythonWindow(id)
{
  m_bRunning = false;
  m_loadOnDemand = false;
}

CGUIPythonWindowDialog::~CGUIPythonWindowDialog(void)
{
}

bool CGUIPythonWindowDialog::OnMessage(CGUIMessage& message)
{
  switch(message.GetMessage())
  {
    case GUI_MSG_WINDOW_INIT:
    {
      CGUIWindow::OnMessage(message);
      return true;
    }
    break;

    case GUI_MSG_CLICKED:
    {
      return CGUIPythonWindow::OnMessage(message);
    }
    break;
  }

  // we do not message CGUIPythonWindow here..
  return CGUIWindow::OnMessage(message);
}

void CGUIPythonWindowDialog::Show(bool show /* = true */)
{
  int count = ExitCriticalSection(g_graphicsContext);
  ThreadMessage tMsg = {TMSG_GUI_PYTHON_DIALOG, 0, show ? 1 : 0};
  tMsg.lpVoid = this;
  g_application.getApplicationMessenger().SendMessage(tMsg, true);
  RestoreCriticalSection(g_graphicsContext, count);
}

void CGUIPythonWindowDialog::Show_Internal(bool show /* = true */)
{
  if (show)
  {
    g_windowManager.RouteToWindow(this);

    // active this dialog...
    CGUIMessage msg(GUI_MSG_WINDOW_INIT,0,0);
    OnMessage(msg);
    m_bRunning = true;
  }
  else // hide
  {
    CGUIMessage msg(GUI_MSG_WINDOW_DEINIT,0,0);
    OnMessage(msg);

    g_windowManager.RemoveDialog(GetID());
    m_bRunning = false;
  }
}

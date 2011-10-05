#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#pragma once

/*
 *      Copyright (C) 2007-2010 Team XBMC
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

//
// C++ Interface: CKeyboard
//
// Description: Adds features like international keyboard layout mapping on top of the
// platform specific low level keyboard classes.
// Here it must be done only once. Within the other mentioned classes it would have to be done several times.
//
// Keyboards alyways deliver printable characters, logical keys for functional behaviour, modifiers ... alongside
// Based on the same hardware with the same scancodes (also alongside) but delivered with different labels to customers
// the software must solve the mapping to the real labels. This is done here.
// The mapping must be specified by an xml configuration that should be able to access everything available,
// but this allows for double/redundant or ambiguous mapping definition, e.g.
// ASCII/unicode could be derived from scancodes, virtual keys, modifiers and/or other ASCII/unicode.

#include "XBMC_events.h"
#include "system.h" // for DWORD

class CKeyboardStat
{
public:
  CKeyboardStat();
  ~CKeyboardStat();

  void Initialize();
  void Reset();
  void ResetState();
  void Update(XBMC_Event& event);
  bool GetShift() { return m_bShift;};
  bool GetCtrl() { return m_bCtrl;};
  bool GetAlt() { return m_bAlt;};
  bool GetRAlt() { return m_bRAlt;};
  bool GetSuper() { return m_bSuper;};
  char GetAscii();// { return m_cAscii;}; // FIXME should be replaced completly by GetUnicode()
  wchar_t GetUnicode();// { return m_wUnicode;};
  uint8_t GetVKey() { return m_VKey;};
  unsigned int KeyHeld() const;

  int HandleEvent(XBMC_Event& newEvent);

private:
  bool m_bShift;
  bool m_bCtrl;
  bool m_bAlt;
  bool m_bRAlt;
  bool m_bSuper;
  char m_cAscii;
  wchar_t m_wUnicode;
  uint8_t m_VKey;

  XBMCKey m_lastKey;
  unsigned int m_lastKeyTime;
  unsigned int m_keyHoldTime;
  bool m_bEvdev;
};

extern CKeyboardStat g_Keyboard;

#endif

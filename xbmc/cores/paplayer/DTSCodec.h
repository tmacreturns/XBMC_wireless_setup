#pragma once

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

#include "FileSystem/File.h"
#include "ICodec.h"
#include "CachingCodec.h"
#include "DllDCACodec.h"

#ifdef USE_LIBDTS_DECODER

class DTSCodec : public CachingCodec
{
public:
  DTSCodec();
  virtual ~DTSCodec();

  virtual bool    Init(const CStdString &strFile, unsigned int filecache);
  virtual void    DeInit();
  virtual __int64 Seek(__int64 iSeekTime);
  virtual int     ReadPCM(BYTE *pBuffer, int size, int *actualsize);
  virtual bool    CanInit();

protected:
  virtual bool CalculateTotalTime();
  virtual int  ReadInput();
  virtual void PrepairBuffers();
  virtual bool InitFile(const CStdString &strFile, unsigned int filecache);
  virtual void CloseFile();
  virtual void SetDefault();
  
  int  Decode(BYTE* pData, int iSize);
  int  GetNrOfChannels(int flags);
  
  static void convert2s16_2(convert_t * _f, int16_t * s16);
  static void convert2s16_3(convert_t * _f, int16_t * s16);
  static void convert2s16_4(convert_t * _f, int16_t * s16);
  static void convert2s16_5(convert_t * _f, int16_t * s16);
  static void convert2s16_multi(convert_t * _f, int16_t * s16, int flags);

  dts_state_t* m_pState;

  BYTE m_inputBuffer[12288];
  BYTE* m_pInputBuffer;

  BYTE* m_readBuffer;
  unsigned int m_readingBufferSize;
  unsigned int m_readBufferPos;

  BYTE* m_decodedData;
  unsigned int m_decodingBufferSize;
  unsigned int m_decodedDataSize;

  bool m_eof;
  int  m_iDataStart;
  bool m_IsInitialized;
  bool m_DecoderError;

  int    m_iFrameSize;
  int    m_iFlags;
  float* m_fSamples;

  int m_iSourceSampleRate;
  int m_iSourceChannels;
  int m_iSourceBitrate;

  int m_iOutputChannels;

  DllDCACodec m_dll;
};
#endif


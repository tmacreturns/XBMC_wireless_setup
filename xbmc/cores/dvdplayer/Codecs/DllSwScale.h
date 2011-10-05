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

#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#endif
#include "DynamicDll.h"
#include "utils/log.h"

extern "C" {
#ifndef HAVE_MMX
#define HAVE_MMX
#endif

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#ifndef __GNUC__
#pragma warning(disable:4244)
#endif

#if (defined USE_EXTERNAL_FFMPEG)
  #if (defined HAVE_LIBAVUTIL_AVUTIL_H)
    #include <libavutil/avutil.h>
  #elif (defined HAVE_FFMPEG_AVUTIL_H)
    #include <ffmpeg/avutil.h>
  #endif
  #if (defined HAVE_LIBSWSCALE_SWSCALE_H)
    #include <libswscale/swscale.h>
  #elif (defined HAVE_FFMPEG_SWSCALE_H)
    #include <ffmpeg/swscale.h>
  #endif
  #if (defined HAVE_LIBSWSCALE_RGB2RGB_H)
    #include <libswscale/rgb2rgb.h>
  #elif (defined HAVE_FFMPEG_RGB2RGB_H)
    #include <ffmpeg/rgb2rgb.h>
  #endif
#else
  #include "libavutil/avutil.h"
  #include "libswscale/swscale.h"
  #include "libswscale/rgb2rgb.h"
#endif
}

inline int SwScaleCPUFlags()
{
#if !defined(__powerpc__) && !defined(__ppc__) && !defined(__arm__)
  return SWS_CPU_CAPS_MMX;
#elif defined(__powerpc__) || defined(__ppc__)
  return SWS_CPU_CAPS_ALTIVEC;
#else
  return 0;
#endif
}

class DllSwScaleInterface
{
public:
   virtual ~DllSwScaleInterface() {}

   virtual struct SwsContext *sws_getCachedContext(struct SwsContext *context,
                                             int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat, int flags,
                                  SwsFilter *srcFilter, SwsFilter *dstFilter, double *param)=0;

   virtual struct SwsContext *sws_getContext(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat, int flags,
                                  SwsFilter *srcFilter, SwsFilter *dstFilter, double *param)=0;

   virtual int sws_scale(struct SwsContext *context, uint8_t* src[], int srcStride[], int srcSliceY,
                         int srcSliceH, uint8_t* dst[], int dstStride[])=0;
    #if (! defined USE_EXTERNAL_FFMPEG)
      virtual void sws_rgb2rgb_init(int flags)=0;
    #elif (defined HAVE_LIBSWSCALE_RGB2RGB_H) || (defined HAVE_FFMPEG_RGB2RGB_H)
      virtual void sws_rgb2rgb_init(int flags)=0;
    #endif

   virtual void sws_freeContext(struct SwsContext *context)=0;
};

#if (defined USE_EXTERNAL_FFMPEG)

// We call into this library directly.
class DllSwScale : public DllDynamic, public DllSwScaleInterface
{
public:
  virtual ~DllSwScale() {}
  virtual struct SwsContext *sws_getCachedContext(struct SwsContext *context,
                                            int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat, int flags,
                               SwsFilter *srcFilter, SwsFilter *dstFilter, double *param) 
    { return ::sws_getCachedContext(context, srcW, srcH, (enum PixelFormat)srcFormat, dstW, dstH, (enum PixelFormat)dstFormat, flags, srcFilter, dstFilter, param); }

  virtual struct SwsContext *sws_getContext(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat, int flags,
                               SwsFilter *srcFilter, SwsFilter *dstFilter, double *param) 
    { return ::sws_getContext(srcW, srcH, (enum PixelFormat)srcFormat, dstW, dstH, (enum PixelFormat)dstFormat, flags, srcFilter, dstFilter, param); }

  virtual int sws_scale(struct SwsContext *context, uint8_t* src[], int srcStride[], int srcSliceY,
                int srcSliceH, uint8_t* dst[], int dstStride[])  
    { return ::sws_scale(context, src, srcStride, srcSliceY, srcSliceH, dst, dstStride); }
  #if (! defined USE_EXTERNAL_FFMPEG)
    virtual void sws_rgb2rgb_init(int flags) { ::sws_rgb2rgb_init(flags); }
  #elif (defined HAVE_LIBSWSCALE_RGB2RGB_H) || (defined HAVE_FFMPEG_RGB2RGB_H)
    virtual void sws_rgb2rgb_init(int flags) { ::sws_rgb2rgb_init(flags); }
  #endif
  virtual void sws_freeContext(struct SwsContext *context) { ::sws_freeContext(context); }
  
  // DLL faking.
  virtual bool ResolveExports() { return true; }
  virtual bool Load() {
    CLog::Log(LOGDEBUG, "DllSwScale: Using libswscale system library");
    return true;
  }
  virtual void Unload() {}
};

#else

class DllSwScale : public DllDynamic, public DllSwScaleInterface
{
  DECLARE_DLL_WRAPPER(DllSwScale, DLL_PATH_LIBSWSCALE)
  DEFINE_METHOD11(struct SwsContext *, sws_getCachedContext, ( struct SwsContext *p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8,
							 SwsFilter * p9, SwsFilter * p10, double * p11))
  DEFINE_METHOD10(struct SwsContext *, sws_getContext, ( int p1, int p2, int p3, int p4, int p5, int p6, int p7, 
							 SwsFilter * p8, SwsFilter * p9, double * p10))
  DEFINE_METHOD7(int, sws_scale, (struct SwsContext *p1, uint8_t** p2, int *p3, int p4, int p5, uint8_t **p6, int *p7))
  DEFINE_METHOD1(void, sws_rgb2rgb_init, (int p1))
  DEFINE_METHOD1(void, sws_freeContext, (struct SwsContext *p1))

  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD(sws_getCachedContext)
    RESOLVE_METHOD(sws_getContext)
    RESOLVE_METHOD(sws_scale)
    RESOLVE_METHOD(sws_rgb2rgb_init)
    RESOLVE_METHOD(sws_freeContext)
  END_METHOD_RESOLVE()
};

#endif

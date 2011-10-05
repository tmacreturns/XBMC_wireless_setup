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
  #if (defined HAVE_LIBAVCODEC_AVCODEC_H)
    #include <libavcodec/avcodec.h>
    #include <libavutil/crc.h>
    #if (defined AVPACKET_IN_AVFORMAT)
      #include <libavformat/avformat.h>
    #endif
  #elif (defined HAVE_FFMPEG_AVCODEC_H)
    #include <ffmpeg/avcodec.h>
    #include <ffmpeg/crc.h>
    #if (defined AVPACKET_IN_AVFORMAT)
      #include <ffmpeg/avformat.h>
    #endif
  #endif
  /* We'll just inlude this header in our project for now */
  #include "xbmc/cores/dvdplayer/Codecs/ffmpeg/libavcodec/audioconvert.h"
  #include "xbmc/cores/dvdplayer/Codecs/ffmpeg/libavutil/crc.h"
#else
  #include "libavcodec/avcodec.h"
  #include "libavcodec/audioconvert.h"
  #include "libavutil/crc.h"
#endif
}

/* Some convenience macros introduced at this particular revision of libavcodec.
 */
#if LIBAVCODEC_VERSION_INT < (52<<16 | 25<<8)
#define CH_LAYOUT_5POINT0_BACK      (CH_LAYOUT_SURROUND|CH_BACK_LEFT|CH_BACK_RIGHT)
#define CH_LAYOUT_5POINT1_BACK      (CH_LAYOUT_5POINT0_BACK|CH_LOW_FREQUENCY)
#undef CH_LAYOUT_7POINT1_WIDE
#define CH_LAYOUT_7POINT1_WIDE      (CH_LAYOUT_5POINT1_BACK|\
                                           CH_FRONT_LEFT_OF_CENTER|CH_FRONT_RIGHT_OF_CENTER)
#endif

#include "utils/SingleLock.h"

class DllAvCodecInterface
{
public:
  virtual ~DllAvCodecInterface() {}
  virtual void avcodec_register_all(void)=0;
  virtual void avcodec_flush_buffers(AVCodecContext *avctx)=0;
  virtual int avcodec_open_dont_call(AVCodecContext *avctx, AVCodec *codec)=0;
  virtual AVCodec *avcodec_find_decoder(enum CodecID id)=0;
  virtual AVCodec *avcodec_find_encoder(enum CodecID id)=0;
  virtual int avcodec_close_dont_call(AVCodecContext *avctx)=0;
  virtual AVFrame *avcodec_alloc_frame(void)=0;
  virtual int avpicture_fill(AVPicture *picture, uint8_t *ptr, PixelFormat pix_fmt, int width, int height)=0;
  virtual int avcodec_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, uint8_t *buf, int buf_size)=0;
  virtual int avcodec_decode_audio2(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, uint8_t *buf, int buf_size)=0;
  virtual int avcodec_decode_subtitle(AVCodecContext *avctx, AVSubtitle *sub, int *got_sub_ptr, const uint8_t *buf, int buf_size)=0;
  virtual int avcodec_encode_audio(AVCodecContext *avctx, uint8_t *buf, int buf_size, const short *samples)=0;
  virtual int avpicture_get_size(PixelFormat pix_fmt, int width, int height)=0;
  virtual AVCodecContext *avcodec_alloc_context(void)=0;
  virtual void avcodec_string(char *buf, int buf_size, AVCodecContext *enc, int encode)=0;
  virtual void avcodec_get_context_defaults(AVCodecContext *s)=0;
  virtual AVCodecParserContext *av_parser_init(int codec_id)=0;
  virtual int av_parser_parse(AVCodecParserContext *s,AVCodecContext *avctx, uint8_t **poutbuf, int *poutbuf_size,
                    const uint8_t *buf, int buf_size,
                    int64_t pts, int64_t dts)=0;
  virtual void av_parser_close(AVCodecParserContext *s)=0;
  virtual void avpicture_free(AVPicture *picture)=0;
  virtual void av_free_packet(AVPacket *pkt)=0;
  virtual int avpicture_alloc(AVPicture *picture, PixelFormat pix_fmt, int width, int height)=0;
  virtual AVOption *av_set_string(void *obj, const char *name, const char *val)=0;
  virtual enum PixelFormat avcodec_default_get_format(struct AVCodecContext *s, const enum PixelFormat *fmt)=0;
  virtual int avcodec_default_get_buffer(AVCodecContext *s, AVFrame *pic)=0;
  virtual void avcodec_default_release_buffer(AVCodecContext *s, AVFrame *pic)=0;
  virtual int avcodec_thread_init(AVCodecContext *s, int thread_count)=0;
  virtual AVCodec *av_codec_next(AVCodec *c)=0;
  virtual int av_get_bits_per_sample_format(enum SampleFormat sample_fmt)=0;
  virtual AVAudioConvert *av_audio_convert_alloc(enum SampleFormat out_fmt, int out_channels,
                                                 enum SampleFormat in_fmt , int in_channels,
                                                 const float *matrix      , int flags)=0;
  virtual void av_audio_convert_free(AVAudioConvert *ctx)=0;
  virtual int av_audio_convert(AVAudioConvert *ctx,
                                     void * const out[6], const int out_stride[6],
                               const void * const  in[6], const int  in_stride[6], int len)=0;
  virtual int av_dup_packet(AVPacket *pkt)=0;
  virtual void av_init_packet(AVPacket *pkt)=0;
  virtual int64_t avcodec_guess_channel_layout(int nb_channels, enum CodecID codec_id, const char *fmt_name)=0;
};

#if (defined USE_EXTERNAL_FFMPEG)

extern "C" { AVOption* av_set_string(void *obj, const char *name, const char *val); }

// Use direct layer
class DllAvCodec : public DllDynamic, DllAvCodecInterface
{
public:
  static CCriticalSection m_critSection;

  virtual ~DllAvCodec() {}
  virtual void avcodec_register_all()
  {
    CSingleLock lock(DllAvCodec::m_critSection);
    ::avcodec_register_all();
  }
  virtual void avcodec_flush_buffers(AVCodecContext *avctx) { ::avcodec_flush_buffers(avctx); }
  virtual int avcodec_open(AVCodecContext *avctx, AVCodec *codec)
  {
    CSingleLock lock(DllAvCodec::m_critSection);
    return ::avcodec_open(avctx, codec);
  }
  virtual int avcodec_open_dont_call(AVCodecContext *avctx, AVCodec *codec) { *(int *)0x0 = 0; return 0; }
  virtual int avcodec_close_dont_call(AVCodecContext *avctx) { *(int *)0x0 = 0; return 0; }
  virtual AVCodec *avcodec_find_decoder(enum CodecID id) { return ::avcodec_find_decoder(id); }
  virtual AVCodec *avcodec_find_encoder(enum CodecID id) { return ::avcodec_find_encoder(id); }
  virtual int avcodec_close(AVCodecContext *avctx)
  {
    CSingleLock lock(DllAvCodec::m_critSection);
    return ::avcodec_close(avctx);
  }
  virtual AVFrame *avcodec_alloc_frame() { return ::avcodec_alloc_frame(); }
  virtual int avpicture_fill(AVPicture *picture, uint8_t *ptr, PixelFormat pix_fmt, int width, int height) { return ::avpicture_fill(picture, ptr, pix_fmt, width, height); }
  virtual int avcodec_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, uint8_t *buf, int buf_size) { return ::avcodec_decode_video(avctx, picture, got_picture_ptr, buf, buf_size); }
  virtual int avcodec_decode_audio2(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, uint8_t *buf, int buf_size) { return ::avcodec_decode_audio2(avctx, samples, frame_size_ptr, buf, buf_size); }
  virtual int avcodec_decode_subtitle(AVCodecContext *avctx, AVSubtitle *sub, int *got_sub_ptr, const uint8_t *buf, int buf_size) { return ::avcodec_decode_subtitle(avctx, sub, got_sub_ptr, buf, buf_size); }
  virtual int avcodec_encode_audio(AVCodecContext *avctx, uint8_t *buf, int buf_size, const short *samples) { return ::avcodec_encode_audio(avctx, buf, buf_size, samples); }
  virtual int avpicture_get_size(PixelFormat pix_fmt, int width, int height) { return ::avpicture_get_size(pix_fmt, width, height); }
  virtual AVCodecContext *avcodec_alloc_context() { return ::avcodec_alloc_context(); }
  virtual void avcodec_string(char *buf, int buf_size, AVCodecContext *enc, int encode) { ::avcodec_string(buf, buf_size, enc, encode); }
  virtual void avcodec_get_context_defaults(AVCodecContext *s) { ::avcodec_get_context_defaults(s); }

  virtual AVCodecParserContext *av_parser_init(int codec_id) { return ::av_parser_init(codec_id); }
  virtual int av_parser_parse(AVCodecParserContext *s,AVCodecContext *avctx, uint8_t **poutbuf, int *poutbuf_size,
                    const uint8_t *buf, int buf_size,
                    int64_t pts, int64_t dts) { return ::av_parser_parse(s, avctx, poutbuf, poutbuf_size, buf, buf_size, pts, dts); }
  virtual void av_parser_close(AVCodecParserContext *s) { ::av_parser_close(s); }

  virtual void avpicture_free(AVPicture *picture) { ::avpicture_free(picture); }
  virtual void av_free_packet(AVPacket *pkt) { ::av_free_packet(pkt); }
  virtual int avpicture_alloc(AVPicture *picture, PixelFormat pix_fmt, int width, int height) { return ::avpicture_alloc(picture, pix_fmt, width, height); }
  virtual AVOption *av_set_string(void *obj, const char *name, const char *val) { return ::av_set_string(obj, name, val); }
  virtual int avcodec_default_get_buffer(AVCodecContext *s, AVFrame *pic) { return ::avcodec_default_get_buffer(s, pic); }
  virtual void avcodec_default_release_buffer(AVCodecContext *s, AVFrame *pic) { ::avcodec_default_release_buffer(s, pic); }
  virtual enum PixelFormat avcodec_default_get_format(struct AVCodecContext *s, const enum PixelFormat *fmt) { return ::avcodec_default_get_format(s, fmt); }
  virtual int avcodec_thread_init(AVCodecContext *s, int thread_count) { return ::avcodec_thread_init(s, thread_count); }
  virtual AVCodec *av_codec_next(AVCodec *c) { return ::av_codec_next(c); }
  virtual int av_get_bits_per_sample_format(enum SampleFormat sample_fmt)
          { return ::av_get_bits_per_sample_format(sample_fmt); }
  virtual AVAudioConvert *av_audio_convert_alloc(enum SampleFormat out_fmt, int out_channels,
                                                 enum SampleFormat in_fmt , int in_channels,
                                                 const float *matrix      , int flags)
          { return ::av_audio_convert_alloc(out_fmt, out_channels, in_fmt, in_channels, matrix, flags); }
  virtual void av_audio_convert_free(AVAudioConvert *ctx)
          { ::av_audio_convert_free(ctx); }

  virtual int av_audio_convert(AVAudioConvert *ctx,
                                     void * const out[6], const int out_stride[6],
                               const void * const  in[6], const int  in_stride[6], int len)
          { return ::av_audio_convert(ctx, out, out_stride, in, in_stride, len); }

  virtual int av_dup_packet(AVPacket *pkt) { return ::av_dup_packet(pkt); }
  virtual void av_init_packet(AVPacket *pkt) { return ::av_init_packet(pkt); }
  virtual int64_t avcodec_guess_channel_layout(int nb_channels, enum CodecID codec_id, const char *fmt_name) { return ::avcodec_guess_channel_layout(nb_channels, codec_id, fmt_name); }

  // DLL faking.
  virtual bool ResolveExports() { return true; }
  virtual bool Load() {
    CLog::Log(LOGDEBUG, "DllAvCodec: Using libavcodec system library");
    return true;
  }
  virtual void Unload() {}
};
#else
class DllAvCodec : public DllDynamic, DllAvCodecInterface
{
  DECLARE_DLL_WRAPPER(DllAvCodec, DLL_PATH_LIBAVCODEC)
#ifndef _LINUX
  DEFINE_FUNC_ALIGNED1(void, __cdecl, avcodec_flush_buffers, AVCodecContext*)
  DEFINE_FUNC_ALIGNED2(int, __cdecl, avcodec_open_dont_call, AVCodecContext*, AVCodec *)
  DEFINE_FUNC_ALIGNED5(int, __cdecl, avcodec_decode_video, AVCodecContext*, AVFrame*, int*, uint8_t*, int)
  DEFINE_FUNC_ALIGNED5(int, __cdecl, avcodec_decode_audio2, AVCodecContext*, int16_t*, int*, uint8_t*, int)
  DEFINE_FUNC_ALIGNED5(int, __cdecl, avcodec_decode_subtitle, AVCodecContext*, AVSubtitle*, int*, const uint8_t *, int)
  DEFINE_FUNC_ALIGNED4(int, __cdecl, avcodec_encode_audio, AVCodecContext*, uint8_t*, int, const short*)
  DEFINE_FUNC_ALIGNED0(AVCodecContext*, __cdecl, avcodec_alloc_context)
  DEFINE_FUNC_ALIGNED1(AVCodecParserContext*, __cdecl, av_parser_init, int)
  DEFINE_FUNC_ALIGNED8(int, __cdecl, av_parser_parse, AVCodecParserContext*,AVCodecContext*, uint8_t**, int*, const uint8_t*, int, int64_t, int64_t)
#else
  DEFINE_METHOD1(void, avcodec_flush_buffers, (AVCodecContext* p1))
  DEFINE_METHOD2(int, avcodec_open_dont_call, (AVCodecContext* p1, AVCodec *p2))
  DEFINE_METHOD5(int, avcodec_decode_video, (AVCodecContext* p1, AVFrame *p2, int *p3, uint8_t *p4, int p5))
  DEFINE_METHOD5(int, avcodec_decode_audio2, (AVCodecContext* p1, int16_t *p2, int *p3, uint8_t *p4, int p5))
  DEFINE_METHOD5(int, avcodec_decode_subtitle, (AVCodecContext* p1, AVSubtitle *p2, int *p3, const uint8_t *p4, int p5))
  DEFINE_METHOD4(int, avcodec_encode_audio, (AVCodecContext* p1, uint8_t *p2, int p3, const short *p4))
  DEFINE_METHOD0(AVCodecContext*, avcodec_alloc_context)
  DEFINE_METHOD1(AVCodecParserContext*, av_parser_init, (int p1))
  DEFINE_METHOD8(int, av_parser_parse, (AVCodecParserContext* p1, AVCodecContext* p2, uint8_t** p3, int* p4, const uint8_t* p5, int p6, int64_t p7, int64_t p8))
#endif
  DEFINE_METHOD1(int, av_dup_packet, (AVPacket *p1))
  DEFINE_METHOD1(void, av_init_packet, (AVPacket *p1))
  DEFINE_METHOD3(int64_t, avcodec_guess_channel_layout, (int p1, enum CodecID p2, const char *p3))

  LOAD_SYMBOLS();

  DEFINE_METHOD0(void, avcodec_register_all_dont_call)
  DEFINE_METHOD1(AVCodec*, avcodec_find_decoder, (enum CodecID p1))
  DEFINE_METHOD1(AVCodec*, avcodec_find_encoder, (enum CodecID p1))
  DEFINE_METHOD1(int, avcodec_close_dont_call, (AVCodecContext *p1))
  DEFINE_METHOD0(AVFrame*, avcodec_alloc_frame)
  DEFINE_METHOD5(int, avpicture_fill, (AVPicture *p1, uint8_t *p2, PixelFormat p3, int p4, int p5))
  DEFINE_METHOD3(int, avpicture_get_size, (PixelFormat p1, int p2, int p3))
  DEFINE_METHOD4(void, avcodec_string, (char *p1, int p2, AVCodecContext *p3, int p4))
  DEFINE_METHOD1(void, avcodec_get_context_defaults, (AVCodecContext *p1))
  DEFINE_METHOD1(void, av_parser_close, (AVCodecParserContext *p1))
  DEFINE_METHOD1(void, avpicture_free, (AVPicture *p1))
  DEFINE_METHOD1(void, av_free_packet, (AVPacket *p1))
  DEFINE_METHOD4(int, avpicture_alloc, (AVPicture *p1, PixelFormat p2, int p3, int p4))
  DEFINE_METHOD3(AVOption*, av_set_string, (void *p1, const char *p2, const char *p3))
  DEFINE_METHOD2(int, avcodec_default_get_buffer, (AVCodecContext *p1, AVFrame *p2))
  DEFINE_METHOD2(void, avcodec_default_release_buffer, (AVCodecContext *p1, AVFrame *p2))
  DEFINE_METHOD2(enum PixelFormat, avcodec_default_get_format, (struct AVCodecContext *p1, const enum PixelFormat *p2))

  DEFINE_METHOD2(int, avcodec_thread_init, (AVCodecContext *p1, int p2))
  DEFINE_METHOD1(AVCodec*, av_codec_next, (AVCodec *p1))
  DEFINE_METHOD1(int, av_get_bits_per_sample_format, (enum SampleFormat p1))
  DEFINE_METHOD6(AVAudioConvert*, av_audio_convert_alloc, (enum SampleFormat p1, int p2,
                                                           enum SampleFormat p3, int p4,
                                                           const float *p5, int p6))
  DEFINE_METHOD1(void, av_audio_convert_free, (AVAudioConvert *p1));
  DEFINE_METHOD6(int,  av_audio_convert,      (AVAudioConvert *p1,
                                                     void * const p2[6], const int p3[6],
                                               const void * const p4[6], const int p5[6], int p6))
  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD(avcodec_flush_buffers)
    RESOLVE_METHOD_RENAME(avcodec_open,avcodec_open_dont_call)
    RESOLVE_METHOD_RENAME(avcodec_close,avcodec_close_dont_call)
    RESOLVE_METHOD(avcodec_find_decoder)
    RESOLVE_METHOD(avcodec_find_encoder)
    RESOLVE_METHOD(avcodec_alloc_frame)
    RESOLVE_METHOD_RENAME(avcodec_register_all, avcodec_register_all_dont_call)
    RESOLVE_METHOD(avpicture_fill)
    RESOLVE_METHOD(avcodec_decode_video)
    RESOLVE_METHOD(avcodec_decode_audio2)
    RESOLVE_METHOD(avcodec_decode_subtitle)
    RESOLVE_METHOD(avcodec_encode_audio)
    RESOLVE_METHOD(avpicture_get_size)
    RESOLVE_METHOD(avcodec_alloc_context)
    RESOLVE_METHOD(avcodec_string)
    RESOLVE_METHOD(avcodec_get_context_defaults)
    RESOLVE_METHOD(av_parser_init)
    RESOLVE_METHOD(av_parser_parse)
    RESOLVE_METHOD(av_parser_close)
    RESOLVE_METHOD(avpicture_free)
    RESOLVE_METHOD(avpicture_alloc)
    RESOLVE_METHOD(av_free_packet)
    RESOLVE_METHOD(av_set_string)
    RESOLVE_METHOD(avcodec_default_get_buffer)
    RESOLVE_METHOD(avcodec_default_release_buffer)
    RESOLVE_METHOD(avcodec_default_get_format)
    RESOLVE_METHOD(avcodec_thread_init)
    RESOLVE_METHOD(av_codec_next)
    RESOLVE_METHOD(av_get_bits_per_sample_format)
    RESOLVE_METHOD(av_audio_convert_alloc)
    RESOLVE_METHOD(av_audio_convert_free)
    RESOLVE_METHOD(av_audio_convert)
    RESOLVE_METHOD(av_dup_packet)
    RESOLVE_METHOD(av_init_packet)
    RESOLVE_METHOD(avcodec_guess_channel_layout)
  END_METHOD_RESOLVE()
public:
    static CCriticalSection m_critSection;
    int avcodec_open(AVCodecContext *avctx, AVCodec *codec)
    {
      CSingleLock lock(DllAvCodec::m_critSection);
      return avcodec_open_dont_call(avctx,codec);
    }
    int avcodec_close(AVCodecContext *avctx)
    {
      CSingleLock lock(DllAvCodec::m_critSection);
      return avcodec_close_dont_call(avctx);
    }
    void avcodec_register_all()
    {
      CSingleLock lock(DllAvCodec::m_critSection);
      avcodec_register_all_dont_call();
    }
};

#endif

// calback used for logging
void ff_avutil_log(void* ptr, int level, const char* format, va_list va);

class DllAvUtilInterface
{
public:
  virtual ~DllAvUtilInterface() {}
  virtual void av_log_set_callback(void (*)(void*, int, const char*, va_list))=0;
  virtual void *av_malloc(unsigned int size)=0;
  virtual void *av_mallocz(unsigned int size)=0;
  virtual void *av_realloc(void *ptr, unsigned int size)=0;
  virtual void av_free(void *ptr)=0;
  virtual void av_freep(void *ptr)=0;
  virtual int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding)=0;
  virtual int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq)=0;
  virtual const AVCRC* av_crc_get_table(AVCRCId crc_id)=0;
  virtual uint32_t av_crc(const AVCRC *ctx, uint32_t crc, const uint8_t *buffer, size_t length)=0;
};

#if (defined USE_EXTERNAL_FFMPEG)

// Use direct layer
class DllAvUtilBase : public DllDynamic, DllAvUtilInterface
{
public:

  virtual ~DllAvUtilBase() {}
   virtual void av_log_set_callback(void (*foo)(void*, int, const char*, va_list)) { ::av_log_set_callback(foo); }
   virtual void *av_malloc(unsigned int size) { return ::av_malloc(size); }
   virtual void *av_mallocz(unsigned int size) { return ::av_mallocz(size); }
   virtual void *av_realloc(void *ptr, unsigned int size) { return ::av_realloc(ptr, size); }
   virtual void av_free(void *ptr) { ::av_free(ptr); }
   virtual void av_freep(void *ptr) { ::av_freep(ptr); }
   virtual int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding d) { return ::av_rescale_rnd(a, b, c, d); }
   virtual int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq) { return ::av_rescale_q(a, bq, cq); }
   virtual const AVCRC* av_crc_get_table(AVCRCId crc_id) { return ::av_crc_get_table(crc_id); }
   virtual uint32_t av_crc(const AVCRC *ctx, uint32_t crc, const uint8_t *buffer, size_t length) { return ::av_crc(ctx, crc, buffer, length); }

   // DLL faking.
   virtual bool ResolveExports() { return true; }
   virtual bool Load() {
     CLog::Log(LOGDEBUG, "DllAvUtilBase: Using libavutil system library");
     return true;
   }
   virtual void Unload() {}
};

#else

class DllAvUtilBase : public DllDynamic, DllAvUtilInterface
{
  DECLARE_DLL_WRAPPER(DllAvUtilBase, DLL_PATH_LIBAVUTIL)

  LOAD_SYMBOLS()

  DEFINE_METHOD1(void, av_log_set_callback, (void (*p1)(void*, int, const char*, va_list)))
  DEFINE_METHOD1(void*, av_malloc, (unsigned int p1))
  DEFINE_METHOD1(void*, av_mallocz, (unsigned int p1))
  DEFINE_METHOD2(void*, av_realloc, (void *p1, unsigned int p2))
  DEFINE_METHOD1(void, av_free, (void *p1))
  DEFINE_METHOD1(void, av_freep, (void *p1))
  DEFINE_METHOD4(int64_t, av_rescale_rnd, (int64_t p1, int64_t p2, int64_t p3, enum AVRounding p4));
  DEFINE_METHOD3(int64_t, av_rescale_q, (int64_t p1, AVRational p2, AVRational p3));
  DEFINE_METHOD1(const AVCRC*, av_crc_get_table, (AVCRCId p1))
  DEFINE_METHOD4(uint32_t, av_crc, (const AVCRC *p1, uint32_t p2, const uint8_t *p3, size_t p4));

  public:
  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD(av_log_set_callback)
    RESOLVE_METHOD(av_malloc)
    RESOLVE_METHOD(av_mallocz)
    RESOLVE_METHOD(av_realloc)
    RESOLVE_METHOD(av_free)
    RESOLVE_METHOD(av_freep)
    RESOLVE_METHOD(av_rescale_rnd)
    RESOLVE_METHOD(av_rescale_q)
    RESOLVE_METHOD(av_crc_get_table)
    RESOLVE_METHOD(av_crc)
  END_METHOD_RESOLVE()
};

#endif

class DllAvUtil : public DllAvUtilBase
{
public:
  virtual bool Load()
  {
    if( DllAvUtilBase::Load() )
    {
      DllAvUtilBase::av_log_set_callback(ff_avutil_log);
      return true;
    }
    return false;
  }
};

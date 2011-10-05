#!/bin/bash 
if [ -d .libs ]
then
rm -r .libs
fi

make distclean

OPTIONS="
--enable-shared \
--enable-memalign-hack \
--enable-gpl \
--enable-w32threads \
--enable-postproc \
--enable-zlib \
--enable-libfaad \
--disable-static \
--disable-altivec \
--disable-muxers \
--disable-encoders \
--disable-debug \
--disable-ffplay \
--disable-ffserver \
--disable-ffmpeg \
--disable-ffprobe \
--disable-devices \
--enable-muxer=spdif \
--enable-muxer=adts \
--enable-encoder=ac3 \
--enable-encoder=aac \
--enable-runtime-cpudetect"

./configure --extra-cflags="-fno-common -I../libfaad2/include -Iinclude/dxva2 -I../libvpx/" --extra-ldflags="-L../../../../../system/players/dvdplayer" ${OPTIONS} &&
 
make -j3 && 
mkdir .libs &&
cp lib*/*.dll .libs/ &&
mv .libs/swscale-0.dll .libs/swscale-0.6.1.dll &&
cp .libs/avcodec-52.dll ../../../../../system/players/dvdplayer/ &&
cp .libs/avformat-52.dll ../../../../../system/players/dvdplayer/ &&
cp .libs/avutil-50.dll ../../../../../system/players/dvdplayer/ &&
cp .libs/postproc-51.dll ../../../../../system/players/dvdplayer/ &&
cp .libs/swscale-0.6.1.dll ../../../../../system/players/dvdplayer/ &&
cp libavutil/avconfig.h include/libavutil/


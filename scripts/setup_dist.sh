#!/bin/bash

ffmpeg_ver="7.0"
c_flags="-s USE_PTHREADS=1 -O3"
ld_flags="-s USE_PTHREADS=1 -O3 -s INITIAL_MEMORY=33554432"
prefix="${SRC}/dist"

# TODO, alias emcc

if [ ! -d "./dist" ]; then
    mkdir ./dist
    mkdir ./dist/tmp
    
    echo "Downloading ffmpeg"
    cd ./dist/tmp
    wget https://ffmpeg.org/releases/ffmpeg-${ffmpeg_ver}.tar.xz
    tar xf ffmpeg-${ffmpeg_ver}.tar.xz
    rm ffmpeg-${ffmpeg_ver}.tar.xz

    echo "Configuring ffmpeg"
    cd ffmpeg-${ffmpeg_ver}
    emconfigure ./configure \
    --prefix="${prefix}" \
    --target-os=none \
    --arch=x86_32 \
    --enable-cross-compile \
    --disable-debug \
    --disable-x86asm \
    --disable-inline-asm \
    --disable-asm \
    --disable-stripping \
    --disable-programs \
    --disable-doc \
    --enable-avcodec \
    --enable-avformat \
    --enable-avfilter \
    --enable-avdevice \
    --enable-avutil \
    --enable-swresample \
    --enable-postproc \
    --enable-swscale \
    --enable-filters \
    --enable-protocol=file \
    --enable-decoder=h264,aac,pcm_s16le \
    --enable-demuxer=mov,matroska \
    --enable-muxer=mp4 \
    --extra-cflags="$c_flags" \
    --extra-cxxflags="$c_flags" \
    --extra-ldflags="$ld_flags" \
    --nm=emnm \
    --ar=emar \
    --cc=emcc \
    --cxx=em++ \
    --ranlib=emranlib \
    --objcc=emcc \
    --dep-cc=emcc

    echo "Emscripting ffmpeg"
    emmake make -j4 && \
    emmake make install

fi

#!/bin/bash

ffmpeg_ver="7.0"
c_flags="-O3 -I./dist/include"
ld_flags="-O3 -L./dist/lib -s INITIAL_MEMORY=33554432"

if [ ! -d "./dist" ]; then
    export SRC=$(pwd)
    export EM_PKG_CONFIG_PATH="${SRC}/dist/lib/pkgconfig"
    prefix="${SRC}/dist"

    echo $SRC
    echo $EM_PKG_CONFIG_PATH
    echo ${prefix}

    mkdir ./dist
    mkdir ./dist/tmp

    cd ./dist/tmp
    echo "Downloading ffmpeg"
    wget https://ffmpeg.org/releases/ffmpeg-${ffmpeg_ver}.tar.xz
    tar xf ffmpeg-${ffmpeg_ver}.tar.xz
    rm ffmpeg-${ffmpeg_ver}.tar.xz

    echo "Configuring ffmpeg"
    cd ./ffmpeg-${ffmpeg_ver}
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
    --disable-pthreads \
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
    --enable-decoder=h264,aac,pcm_s16le,libvpx-vp9 \
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

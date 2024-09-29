# emsc-ffmpeg
Client side transcoding video (.mp4, .mov, .mkv) to .mp4 using ffmpeg libraries on the web.

## Overview
- `web/` contains all of the files that will be used to render website
- `test/` contains code to run a local server, use `npm run dev` to start this server
- `scripts/` contains build script to generate libvpx, x264, ffmpeg wasm object files
- `app/` contains `main.cc`

## Dependencies
```sh
brew install pkg-config emscripten
```

## Build and Run Project
```sh
make init && make
npm i && npm run dev
```
# emsc-ffmpeg
Client side transcoding video (.mp4, .mov, .mkv) to .mp4 using ffmpeg libraries to meet an 8MB size limit

## Overview
- `web/` contains all of the files that will be used to render website
- `test/` contains code to run a local server, use `npm run dev` to start this server
- `app/` contains CXX code

## Dependencies
```sh
brew install pkg-config emscripten
```

## Build and Run Project
```sh
npm i
make init && make
npm run dev
```
# emsc-ffmpeg basics
Basic code to get started with Emscripten and ffmpeg

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
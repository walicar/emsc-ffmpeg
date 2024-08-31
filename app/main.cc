#include <emscripten.h>
#include <emscripten/bind.h>
#include <inttypes.h>

#include <string>
#include <vector>

using namespace emscripten;

extern "C"
{
#include <libavformat/avformat.h>
};

typedef struct Response
{
  std::string format;
  int duration;
  int streams;
} Response;

static AVFormatContext *fmt_ctx;

Response parse(std::string filename)
{
  int ret;
  if ((ret = avformat_open_input(&fmt_ctx, filename.c_str(), NULL, NULL)) < 0)
  {
    printf("%s", av_err2str(ret));
  }

  printf("format: %s, duration: %lld us, streams: %d\n",
         fmt_ctx->iformat->name,
         fmt_ctx->duration,
         fmt_ctx->nb_streams);

  Response r = {
      .format = fmt_ctx->iformat->name,
      .duration = (int)fmt_ctx->duration,
      .streams = (int)fmt_ctx->nb_streams,
  };

  return r;
}

EMSCRIPTEN_BINDINGS(ffmpeg_emsc)
{
  emscripten::value_object<Response>("Response")
      .field("format", &Response::format)
      .field("duration", &Response::duration)
      .field("streams", &Response::streams);
  function("Parse", &parse);
}

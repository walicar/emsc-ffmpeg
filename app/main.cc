#include <emscripten.h>
#include <emscripten/bind.h>
#include <inttypes.h>

#include <string>
#include <vector>

using namespace emscripten;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/defs.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
#include <libavutil/log.h>
};

const int DST_WIDTH = 1280;
const int DST_HEIGHT = 720;

struct SwsContext *sws_ctx;
int vid_idx = 0;

typedef struct MediaContext {
  AVFormatContext *avfc;
  const AVCodec *avc;
  AVCodecContext *avcc;
  AVStream *avs;
} MediaContext;

MediaContext *encoder;
MediaContext *decoder;

char entry[512];
bool is_callback_set = false;

EM_JS(void, call_log, (const char *text), {
  console.log(UTF8ToString(text));
});

void custom_log_callback(void* ptr, int level, const char* fmt, va_list vargs) {
    vsnprintf(entry, sizeof(entry), fmt, vargs);
    call_log(entry);
}

int init(const char *filename) {
  decoder = (MediaContext *)malloc(sizeof(MediaContext));
  encoder = (MediaContext *)malloc(sizeof(MediaContext));

  if (avformat_open_input(&decoder->avfc, filename, NULL, NULL) < 0) {
    printf("Could not open input file to get format\n");
    return -1;
  }

  if (avformat_find_stream_info(decoder->avfc, NULL) < 0) {
    printf("Could not find input file stream info\n");
    return -1;
  }

  vid_idx = av_find_best_stream(decoder->avfc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  decoder->avs = decoder->avfc->streams[vid_idx];
  AVCodecParameters *params = decoder->avfc->streams[vid_idx]->codecpar;

  decoder->avc = avcodec_find_decoder(params->codec_id);
  if (!decoder->avc) {
    printf("Cannot find decoder %d\n", params->codec_id);
    return -1;
  }

  decoder->avcc = avcodec_alloc_context3(decoder->avc);
  if (!decoder->avcc) {
    printf("Could not allocate decoder context\n");
    return -1;
  }

  if (avcodec_parameters_to_context(decoder->avcc, params) < 0) {
    printf("Could not copy decoder params to decoder context\n");
    return -1;
  }

  decoder->avcc->pkt_timebase = decoder->avs->time_base;
  decoder->avcc->framerate = av_guess_frame_rate(NULL, decoder->avs, NULL);

  if (avcodec_open2(decoder->avcc, decoder->avc, NULL) < 0) {
    printf("Could not open decoder\n");
    return -1;
  }

  // setup output
  if (avformat_alloc_output_context2(&encoder->avfc, NULL, NULL, "output.mp4") <
      0) {
    printf("Could not open output file to get format\n");
    return -1;
  }

  encoder->avc = avcodec_find_encoder_by_name("libx264");
  if (!encoder->avc) {
    printf("Cannot find encoder %d\n", params->codec_id);
    return -1;
  }

  encoder->avcc = avcodec_alloc_context3(encoder->avc);
  if (!encoder->avcc) {
    printf("Could not allocate encoder context\n");
    return -1;
  }

  encoder->avcc->bit_rate = 1000;
  encoder->avcc->width = DST_WIDTH;
  encoder->avcc->height = DST_HEIGHT;
  encoder->avcc->time_base = av_inv_q(decoder->avcc->framerate);
  encoder->avcc->framerate = decoder->avcc->framerate;
  encoder->avs->time_base = encoder->avcc->time_base;
  encoder->avcc->pix_fmt = AV_PIX_FMT_YUV420P;

  if (encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER)
                encoder->avcc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  if (avcodec_open2(encoder->avcc, encoder->avc, NULL) < 0) {
    printf("Could not open encoder\n");
    return -1;
  }

  encoder->avs = avformat_new_stream(encoder->avfc, NULL);
  if (avio_open(&encoder->avfc->pb, "output.mp4", AVIO_FLAG_WRITE) < 0) {
    printf("Could not create output video stream\n");
    return -1;
  }

  // Copy the codec parameters from the codec context to the stream's codecpar
  if (avcodec_parameters_from_context(encoder->avs->codecpar, encoder->avcc) <
      0) {
    printf("Failed to copy codec parameters to output stream\n");
    return -1;
  }

  if (avformat_write_header(encoder->avfc, NULL) < 0) {
    printf("Could not write header for output file\n");
    return -1;
  }

  // setup sws
  sws_ctx = sws_getContext(decoder->avcc->width, decoder->avcc->height,
                           decoder->avcc->pix_fmt, encoder->avcc->width,
                           encoder->avcc->height, AV_PIX_FMT_YUV420P,
                           SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws_ctx) {
    printf("Could not get sws context\n");
    return -1;
  }

  return 0;
}

int _transcode(std::string filename) {
  if (!is_callback_set)
    av_log_set_callback(custom_log_callback);

  if (init(filename.c_str()) < 0) return 1;

  AVFrame *dec_frame = av_frame_alloc();
  AVFrame *enc_frame = av_frame_alloc();
  enc_frame->format = AV_PIX_FMT_YUV420P;
  enc_frame->width = encoder->avcc->width;
  enc_frame->height = encoder->avcc->height;
  av_frame_get_buffer(enc_frame, 32);

  AVPacket *dec_pkt = av_packet_alloc();
  AVPacket *enc_pkt = av_packet_alloc();

  while (av_read_frame(decoder->avfc, dec_pkt) >= 0) {
    if (dec_pkt->stream_index == vid_idx) {
      if (avcodec_send_packet(decoder->avcc, dec_pkt) < 0) {
        printf("Could not send packet to decoder, cleaning up...\n");
        break;
      }

      int rf_res = 0;
      while (rf_res >= 0) {
        rf_res = avcodec_receive_frame(decoder->avcc, dec_frame);
        if (rf_res == AVERROR(EAGAIN) || rf_res == AVERROR_EOF) {
          break;
        } else if (rf_res < 0) {
          printf("Decoder could not receive frame\n");
          break;
        }

        sws_scale(sws_ctx, (const uint8_t *const *)dec_frame->data,
                  dec_frame->linesize, 0, dec_frame->height, enc_frame->data,
                  enc_frame->linesize);

        enc_frame->pts = dec_frame->pts;

        if (avcodec_send_frame(encoder->avcc, enc_frame) < 0) {
          printf("Could not send frame to encoder\n");
          break;
        }

        int rp_res = 0;
        while (rp_res >= 0) {
          rp_res = avcodec_receive_packet(encoder->avcc, enc_pkt);
          if (rp_res == AVERROR(EAGAIN) || rp_res == AVERROR_EOF) {
            break;
          } else if (rp_res < 0) {
            printf("Encoder could not receive encoded packet\n");
            break;
          }

          enc_pkt->stream_index = 0;
          av_packet_rescale_ts(enc_pkt, decoder->avs->time_base, encoder->avs->time_base); // important

          if (av_write_frame(encoder->avfc, enc_pkt) < 0) {
            printf("Could not write frame to encoded packet\n");
            return -1;
          }

          av_packet_unref(enc_pkt);
        }
      }
      av_packet_unref(dec_pkt);
    }
  }

  avcodec_send_packet(decoder->avcc, NULL);
  while (avcodec_receive_frame(decoder->avcc, dec_frame) >= 0) {
    sws_scale(sws_ctx, (const uint8_t *const *)dec_frame->data,
              dec_frame->linesize, 0, dec_frame->height, enc_frame->data,
              enc_frame->linesize);
    enc_frame->pts = dec_frame->pts;
    avcodec_send_frame(encoder->avcc, enc_frame);
    while (avcodec_receive_packet(encoder->avcc, enc_pkt) >= 0) {
      enc_pkt->stream_index = encoder->avs->index;
      av_packet_rescale_ts(enc_pkt, decoder->avs->time_base, encoder->avs->time_base);
      av_write_frame(encoder->avfc, enc_pkt);
      av_packet_unref(enc_pkt);
    }
  }

  avcodec_send_frame(encoder->avcc, NULL);
  while (avcodec_receive_packet(encoder->avcc, enc_pkt) >= 0) {
    enc_pkt->stream_index = encoder->avs->index;
    av_packet_rescale_ts(enc_pkt, decoder->avs->time_base, encoder->avs->time_base);
    av_write_frame(encoder->avfc, enc_pkt);
    av_packet_unref(enc_pkt);
  }

  av_write_trailer(encoder->avfc);

  avio_closep(&encoder->avfc->pb);
  avformat_close_input(&decoder->avfc);
  avformat_close_input(&encoder->avfc);
  av_packet_free(&dec_pkt);
  av_packet_free(&enc_pkt);
  av_frame_free(&dec_frame);
  av_frame_free(&enc_frame);
  avcodec_free_context(&decoder->avcc);
  avcodec_free_context(&encoder->avcc);
  sws_freeContext(sws_ctx);
  free(encoder);
  free(decoder);
  return 0;
}

EMSCRIPTEN_BINDINGS(ffmpeg_emsc) {
  emscripten::function("transcode", &_transcode);
};

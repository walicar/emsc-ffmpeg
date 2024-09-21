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
#include <libavutil/log.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
};

const int DST_WIDTH = 1280;
const int DST_HEIGHT = 720;

struct SwsContext* sws_ctx;
int vid_idx = 0;
int aud_idx = 1;

typedef struct MediaContext {
  AVFormatContext* avfc;

  const AVCodec* video_avc;
  AVCodecContext* video_avcc;
  AVStream* video_avs;

  const AVCodec* audio_avc;
  AVCodecContext* audio_avcc;
  AVStream* audio_avs;
} MediaContext;

MediaContext* encoder;
MediaContext* decoder;

char entry[512];
bool is_callback_set = false;

EM_JS(void, call_log, (const char* text), { console.log(UTF8ToString(text)); });

void custom_log_callback(void* ptr, int level, const char* fmt, va_list vargs) {
  vsnprintf(entry, sizeof(entry), fmt, vargs);
  call_log(entry);
}

int init(const char* filename) {
  decoder = (MediaContext*)malloc(sizeof(MediaContext));
  encoder = (MediaContext*)malloc(sizeof(MediaContext));

  // setup input
  if (avformat_open_input(&decoder->avfc, filename, NULL, NULL) < 0) {
    printf("Could not open input file to get format\n");
    return -1;
  }

  if (avformat_find_stream_info(decoder->avfc, NULL) < 0) {
    printf("Could not find input file stream info\n");
    return -1;
  }

  vid_idx =
      av_find_best_stream(decoder->avfc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  aud_idx =
      av_find_best_stream(decoder->avfc, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  decoder->video_avs = decoder->avfc->streams[vid_idx];
  decoder->audio_avs = decoder->avfc->streams[aud_idx];

  // decoder video
  AVCodecParameters* params = decoder->video_avs->codecpar;

  decoder->video_avc = avcodec_find_decoder(params->codec_id);
  if (!decoder->video_avc) {
    printf("Cannot find decoder %d\n", params->codec_id);
    return -1;
  }

  decoder->video_avcc = avcodec_alloc_context3(decoder->video_avc);
  if (!decoder->video_avcc) {
    printf("Could not allocate decoder context\n");
    return -1;
  }

  if (avcodec_parameters_to_context(decoder->video_avcc, params) < 0) {
    printf("Could not copy decoder params to decoder context\n");
    return -1;
  }

  decoder->video_avcc->pkt_timebase = decoder->video_avs->time_base;
  decoder->video_avcc->framerate =
      av_guess_frame_rate(NULL, decoder->video_avs, NULL);

  if (avcodec_open2(decoder->video_avcc, decoder->video_avc, NULL) < 0) {
    printf("Could not open decoder\n");
    return -1;
  }

  // decoder audio
  params = decoder->audio_avs->codecpar;

  decoder->audio_avc = avcodec_find_decoder(params->codec_id);
  if (!decoder->audio_avc) {
    printf("Cannot find decoder %d\n", params->codec_id);
    return -1;
  }

  decoder->audio_avcc = avcodec_alloc_context3(decoder->audio_avc);
  if (!decoder->audio_avcc) {
    printf("Could not allocate decoder context\n");
    return -1;
  }

  if (avcodec_parameters_to_context(decoder->audio_avcc, params) < 0) {
    printf("Could not copy decoder params to decoder context\n");
    return -1;
  }

  if (avcodec_open2(decoder->audio_avcc, decoder->audio_avc, NULL) < 0) {
    printf("Could not open decoder\n");
    return -1;
  }

  // setup output
  if (avformat_alloc_output_context2(&encoder->avfc, NULL, NULL, "output.mp4") <
      0) {
    printf("Could not open output file to get format\n");
    return -1;
  }

  // encoder video
  encoder->video_avc = avcodec_find_encoder_by_name("libx264");
  if (!encoder->video_avc) {
    printf("Cannot find encoder %d\n", params->codec_id);
    return -1;
  }

  encoder->video_avcc = avcodec_alloc_context3(encoder->video_avc);
  if (!encoder->video_avcc) {
    printf("Could not allocate encoder context\n");
    return -1;
  }

  encoder->video_avcc->bit_rate = 1000;
  encoder->video_avcc->width = DST_WIDTH;
  encoder->video_avcc->height = DST_HEIGHT;
  encoder->video_avcc->time_base = av_inv_q(decoder->video_avcc->framerate);
  encoder->video_avcc->framerate = decoder->video_avcc->framerate;
  encoder->video_avs->time_base = encoder->video_avcc->time_base;
  encoder->video_avcc->pix_fmt = AV_PIX_FMT_YUV420P;

  if (avcodec_open2(encoder->video_avcc, encoder->video_avc, NULL) < 0) {
    printf("Could not open encoder\n");
    return -1;
  }
  encoder->video_avs = avformat_new_stream(encoder->avfc, NULL);

  if (avcodec_parameters_from_context(encoder->video_avs->codecpar,
                                      encoder->video_avcc) < 0) {
    printf("Failed to copy video codec parameters to output stream\n");
    return -1;
  }

  // encoder audio
  encoder->audio_avc = avcodec_find_encoder_by_name("aac");
  if (!encoder->video_avc) {
    printf("Cannot find encoder %d\n", params->codec_id);
    return -1;
  }

  encoder->audio_avcc = avcodec_alloc_context3(encoder->audio_avc);
  if (!encoder->audio_avcc) {
    printf("Could not allocate encoder context\n");
    return -1;
  }

  encoder->audio_avcc->ch_layout = decoder->audio_avcc->ch_layout;
  encoder->audio_avcc->sample_rate = decoder->audio_avcc->sample_rate;
  encoder->audio_avcc->sample_fmt = encoder->audio_avc->sample_fmts[0];
  encoder->audio_avcc->bit_rate = 98000;
  encoder->audio_avcc->time_base = decoder->audio_avcc->time_base;
  encoder->audio_avcc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
  encoder->audio_avs->time_base = encoder->audio_avcc->time_base;

  if (avcodec_open2(encoder->audio_avcc, encoder->audio_avc, NULL) < 0) {
    printf("Could not open encoder\n");
    return -1;
  }
  encoder->audio_avs = avformat_new_stream(encoder->avfc, NULL);

  if (avcodec_parameters_from_context(encoder->audio_avs->codecpar,
                                      encoder->audio_avcc) < 0) {
    printf("Failed to copy audio codec parameters to output stream\n");
    return -1;
  }

  // wrap up
  if (encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER)
    encoder->avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  if (avio_open(&encoder->avfc->pb, "output.mp4", AVIO_FLAG_WRITE) < 0) {
    printf("Could not create output video stream\n");
    return -1;
  }

  if (avformat_write_header(encoder->avfc, NULL) < 0) {
    printf("Could not write header for output file\n");
    return -1;
  }

  // setup sws
  sws_ctx =
      sws_getContext(decoder->video_avcc->width, decoder->video_avcc->height,
                     decoder->video_avcc->pix_fmt, encoder->video_avcc->width,
                     encoder->video_avcc->height, AV_PIX_FMT_YUV420P,
                     SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws_ctx) {
    printf("Could not get sws context\n");
    return -1;
  }

  return 0;
}

void encode_video(MediaContext* encoder,
                  AVRational dec_time_base,
                  AVPacket* enc_pkt,
                  AVFrame* enc_frame) {
  if (avcodec_send_frame(encoder->video_avcc, enc_frame) < 0) {
    printf("Could not send frame to video encoder\n");
    return;
  }

  int rp_res = 0;
  while (rp_res >= 0) {
    rp_res = avcodec_receive_packet(encoder->video_avcc, enc_pkt);
    if (rp_res == AVERROR(EAGAIN) || rp_res == AVERROR_EOF) {
      break;
    } else if (rp_res < 0) {
      printf("Video encoder could not receive encoded packet\n");
      break;
    }

    enc_pkt->stream_index = 0;
    av_packet_rescale_ts(enc_pkt, dec_time_base,
                         encoder->video_avs->time_base);  // important

    if (av_write_frame(encoder->avfc, enc_pkt) < 0) {
      printf("Could not write video frame to encoded packet\n");
      return;
    }
  }

  av_packet_unref(enc_pkt);
}

void transcode_video(MediaContext* decoder,
                     MediaContext* encoder,
                     AVPacket* dec_pkt,
                     AVFrame* dec_frame,
                     AVPacket* enc_pkt,
                     AVFrame* enc_frame) {
  if (avcodec_send_packet(decoder->video_avcc, dec_pkt) < 0) {
    printf("Could not send packet to video decoder, cleaning up...\n");
    return;
  }

  int rf_res = 0;
  while (rf_res >= 0) {
    rf_res = avcodec_receive_frame(decoder->video_avcc, dec_frame);
    if (rf_res == AVERROR(EAGAIN) || rf_res == AVERROR_EOF) {
      break;
    } else if (rf_res < 0) {
      printf("Video decoder could not receive frame\n");
      break;
    }

    sws_scale(sws_ctx, (const uint8_t* const*)dec_frame->data,
              dec_frame->linesize, 0, dec_frame->height, enc_frame->data,
              enc_frame->linesize);

    enc_frame->pts = dec_frame->pts;
    encode_video(encoder, decoder->video_avs->time_base, enc_pkt, enc_frame);
    av_frame_unref(dec_frame);
  }
  av_packet_unref(dec_pkt);
}

void encode_audio(MediaContext* encoder,
                  AVRational dec_time_base,
                  AVPacket* enc_pkt,
                  AVFrame* enc_frame) {
  if (avcodec_send_frame(encoder->video_avcc, enc_frame) < 0) {
    printf("Could not send frame to audio encoder\n");
    return;
  }

  int rp_res = 0;
  while (rp_res >= 0) {
    rp_res = avcodec_receive_packet(encoder->video_avcc, enc_pkt);
    if (rp_res == AVERROR(EAGAIN) || rp_res == AVERROR_EOF) {
      break;
    } else if (rp_res < 0) {
      printf("Audio encoder could not receive encoded packet\n");
      break;
    }

    enc_pkt->stream_index = 1;
    av_packet_rescale_ts(enc_pkt, dec_time_base, encoder->audio_avs->time_base);

    if (av_write_frame(encoder->avfc, enc_pkt) < 0) {
      printf("Could not write audio frame to encoded packet\n");
      return;
    }
  }
  av_packet_unref(enc_pkt);
}

void transcode_audio(MediaContext* decoder,
                     MediaContext* encoder,
                     AVPacket* dec_pkt,
                     AVFrame* dec_frame,
                     AVPacket* enc_pkt,
                     AVFrame* enc_frame) {
  if (avcodec_send_packet(decoder->audio_avcc, dec_pkt) < 0) {
    printf("Could not send packet to audio decoder\n");
    return;
  }

  int rf_res = 0;
  while (rf_res >= 0) {
    rf_res = avcodec_receive_frame(decoder->audio_avcc, dec_frame);
    if (rf_res == AVERROR(EAGAIN) || rf_res == AVERROR_EOF) {
      break;
    } else if (rf_res < 0) {
      printf("Audio decoder could not receive frame\n");
    }

    enc_frame->pts = dec_frame->pts;
    encode_audio(encoder, decoder->audio_avs->time_base, enc_pkt, enc_frame);
    av_frame_unref(dec_frame);
  }
  av_packet_unref(dec_pkt);
}

int _transcode(std::string filename) {
  if (!is_callback_set)
    av_log_set_callback(custom_log_callback);

  if (init(filename.c_str()) < 0)
    return 1;

  AVFrame* dec_frame = av_frame_alloc();
  AVFrame* enc_frame = av_frame_alloc();
  enc_frame->format = AV_PIX_FMT_YUV420P;
  enc_frame->width = encoder->video_avcc->width;
  enc_frame->height = encoder->video_avcc->height;
  av_frame_get_buffer(enc_frame, 32);

  AVPacket* dec_pkt = av_packet_alloc();
  AVPacket* enc_pkt = av_packet_alloc();

  while (av_read_frame(decoder->avfc, dec_pkt) >= 0) {
    if (dec_pkt->stream_index == vid_idx) {
      transcode_video(decoder, encoder, dec_pkt, dec_frame, enc_pkt, enc_frame);
    } else if (dec_pkt->stream_index == aud_idx) {
      transcode_audio(decoder, encoder, dec_pkt, dec_frame, enc_pkt, enc_frame);
    }
  }

  transcode_video(decoder, encoder, NULL, dec_frame, enc_pkt,
                  enc_frame);  // flush video decoder
  encode_video(encoder, decoder->video_avs->time_base, NULL,
               enc_frame);  // flush video encoder
  transcode_audio(decoder, encoder, NULL, dec_frame, enc_pkt,
                  enc_frame);  // flush audio decoder
  encode_audio(encoder, decoder->video_avs->time_base, NULL,
               enc_frame);  // flush audio encoder

  av_write_trailer(encoder->avfc);

  avio_closep(&encoder->avfc->pb);
  avformat_close_input(&decoder->avfc);
  avformat_close_input(&encoder->avfc);
  av_packet_free(&dec_pkt);
  av_packet_free(&enc_pkt);
  av_frame_free(&dec_frame);
  av_frame_free(&enc_frame);
  avcodec_free_context(&decoder->video_avcc);
  avcodec_free_context(&encoder->video_avcc);
  sws_freeContext(sws_ctx);
  free(encoder);
  free(decoder);
  return 0;
}

EMSCRIPTEN_BINDINGS(emsc_ffmpeg) {
  emscripten::function("transcode", &_transcode);
};

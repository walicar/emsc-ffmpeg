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
#include <libswscale/swscale.h>
};

const int DST_WIDTH = 1280;
const int DST_HEIGHT = 720;

AVStream *out_stream;
struct SwsContext *sws_ctx;
int vid_idx = 0;

typedef struct MediaContext {
  AVFormatContext *avfc;
  const AVCodec *avc;
  AVCodecContext *avcc;
} MediaContext;

MediaContext *encoder;
MediaContext *decoder;

int init(const char *filename) {
  decoder = (MediaContext *)malloc(sizeof(MediaContext));
  encoder = (MediaContext *)malloc(sizeof(MediaContext));

  if (avformat_open_input(&decoder->avfc, filename, NULL, NULL) < 0) {
    fprintf(stderr, "Could not open input file to get format\n");
    exit(1);
  }

  if (avformat_find_stream_info(decoder->avfc, NULL) < 0) {
    fprintf(stderr, "Could not find input file stream info\n");
    exit(1);
  }

  vid_idx = av_find_best_stream(decoder->avfc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  AVCodecParameters *params = decoder->avfc->streams[vid_idx]->codecpar;

  decoder->avc = avcodec_find_decoder(params->codec_id);
  if (!decoder->avc) {
    fprintf(stderr, "Cannot find decoder %d\n", params->codec_id);
    exit(1);
  }

  decoder->avcc = avcodec_alloc_context3(decoder->avc);
  if (!decoder->avcc) {
    fprintf(stderr, "Could not allocate decoder context\n");
    exit(1);
  }

  if (avcodec_parameters_to_context(decoder->avcc, params) < 0) {
    fprintf(stderr, "Could not copy decoder params to decoder context\n");
    exit(1);
  }

  if (avcodec_open2(decoder->avcc, decoder->avc, NULL) < 0) {
    fprintf(stderr, "Could not open decoder\n");
    exit(1);
  }

  // setup output
  if (avformat_alloc_output_context2(&encoder->avfc, NULL, NULL, "output.mp4") < 0) {
    fprintf(stderr, "Could not open output file to get format\n");
    exit(1);
  }

  encoder->avc = avcodec_find_encoder_by_name("libx264");
  if (!encoder->avc) {
    fprintf(stderr, "Cannot find encoder %d\n", params->codec_id);
    exit(1);
  }

  encoder->avcc = avcodec_alloc_context3(encoder->avc);
  if (!encoder->avcc) {
    fprintf(stderr, "Could not allocate encoder context\n");
    exit(1);
  }

  encoder->avcc->bit_rate = 1000000;
  encoder->avcc->width = DST_WIDTH;
  encoder->avcc->height = DST_WIDTH;
  encoder->avcc->time_base = (AVRational){1, 24};
  encoder->avcc->pix_fmt = AV_PIX_FMT_YUV420P;

  if (avcodec_open2(encoder->avcc, encoder->avc, NULL) < 0) {
    fprintf(stderr, "Could not open encoder\n");
    exit(1);
  }

  out_stream = avformat_new_stream(encoder->avfc, NULL);
  if (avio_open(&encoder->avfc->pb, "output.mp4", AVIO_FLAG_WRITE) < 0) {
    fprintf(stderr, "Could not create output video stream\n");
    exit(1);
  }

  // Copy the codec parameters from the codec context to the stream's codecpar
  if (avcodec_parameters_from_context(out_stream->codecpar, encoder->avcc) < 0) {
    fprintf(stderr, "Failed to copy codec parameters to output stream\n");
    exit(1);
  }

  if (avformat_write_header(encoder->avfc, NULL) < 0) {
    fprintf(stderr, "Could not write header for output file\n");
    exit(1);
  }

  // setup sws
  sws_ctx = sws_getContext(decoder->avcc->width, decoder->avcc->height, decoder->avcc->pix_fmt,
                           encoder->avcc->width, encoder->avcc->height, AV_PIX_FMT_YUV420P,
                           SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws_ctx) {
    fprintf(stderr, "Could not get sws context\n");
    exit(1);
  }

  return 0;
}

int _transcode(std::string filename) {
  if (init(filename.c_str()) < 0) {
    return 1;
  }

  AVFrame *dec_frame = av_frame_alloc();
  AVFrame *enc_frame = av_frame_alloc();
  enc_frame->format = AV_PIX_FMT_YUV420P;
  enc_frame->width = encoder->avcc->width;
  enc_frame->height = encoder->avcc->height;
  av_frame_get_buffer(enc_frame, 32);

  AVPacket *dec_pkt = av_packet_alloc();
  AVPacket *enc_pkt = av_packet_alloc();

  int64_t pts = 0; // Manual PTS counter
  while (av_read_frame(decoder->avfc, dec_pkt) >= 0) {
    if (dec_frame->pts == AV_NOPTS_VALUE) {
      dec_frame->pts = pts++;
    }

    dec_frame->pts = av_rescale_q(dec_frame->pts, decoder->avfc->streams[vid_idx]->time_base, encoder->avcc->time_base);
    if (dec_pkt->stream_index == vid_idx) {
      if (avcodec_send_packet(decoder->avcc, dec_pkt) < 0) {
        fprintf(stderr, "Could not send packet to decoder, cleaning up...\n");
        break; // Only break if there's an error
      }

      int rf_res = 0;
      while (rf_res >= 0) {
        rf_res = avcodec_receive_frame(decoder->avcc, dec_frame);
        if (rf_res == AVERROR(EAGAIN) || rf_res == AVERROR_EOF) {
          break;
        } else if (rf_res < 0) {
          fprintf(stderr, "Decoder could not receive frame\n");
          break;
        }

        // Perform scaling (sws_scale)
        sws_scale(sws_ctx, (const uint8_t *const *)dec_frame->data,
                  dec_frame->linesize, 0, dec_frame->height, enc_frame->data,
                  enc_frame->linesize);

        // Send frame to encoder
        if (avcodec_send_frame(encoder->avcc, enc_frame) < 0) {
          fprintf(stderr, "Could not send frame to encoder\n");
          break;
        }

        // Receive encoded packets and write to file
        int rp_res = 0;
        while (rp_res >= 0) {
          rp_res = avcodec_receive_packet(encoder->avcc, enc_pkt);
          if (rp_res == AVERROR(EAGAIN) || rp_res == AVERROR_EOF) {
            break;
          } else if (rp_res < 0) {
            fprintf(stderr, "Encoder could not receive encoded packet\n");
            break;
          }
          av_packet_rescale_ts(enc_pkt, encoder->avcc->time_base, out_stream->time_base);

          enc_pkt->stream_index = out_stream->index;

          if (av_write_frame(encoder->avfc, enc_pkt) < 0) {
            fprintf(stderr, "Could not write frame to encoded packet\n");
            exit(1);
          }

          av_packet_unref(enc_pkt);
        }
      }
      av_packet_unref(dec_pkt);
    }
  }

  // Flush the decoder and encoder after the input file is fully read
  avcodec_send_packet(decoder->avcc, NULL); // Flush decoder
  while (avcodec_receive_frame(decoder->avcc, dec_frame) >= 0) {
    // Scale and encode remaining frames
    sws_scale(sws_ctx, (const uint8_t *const *)dec_frame->data,
              dec_frame->linesize, 0, dec_frame->height, enc_frame->data,
              enc_frame->linesize);

    avcodec_send_frame(encoder->avcc, enc_frame);
    while (avcodec_receive_packet(encoder->avcc, enc_pkt) >= 0) {
      enc_pkt->stream_index = out_stream->index;
      av_write_frame(encoder->avfc, enc_pkt);
      av_packet_unref(enc_pkt);
    }
  }

  avcodec_send_frame(encoder->avcc, NULL); // Flush encoder
  while (avcodec_receive_packet(encoder->avcc, enc_pkt) >= 0) {
    enc_pkt->stream_index = out_stream->index;
    av_write_frame(encoder->avfc, enc_pkt);
    av_packet_unref(enc_pkt);
  }

  // Finalize writing
  av_write_trailer(encoder->avfc);

  // cleanup
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
  return 0;
}

EMSCRIPTEN_BINDINGS(ffmpeg_emsc) {
  emscripten::function("transcode", &_transcode);
};

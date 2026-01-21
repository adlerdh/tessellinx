#include "video.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#include <iostream>

int createAndSaveVideo(const char* filename,
                       int width,
                       int height,
                       int fps,
                       const std::vector<std::vector<uint8_t>>& frames) {

  if (frames.empty()) {
    std::cerr << "Error: No frames provided\n";
    return 1;
  }

  size_t expected_frame_size = width * height * 3;
  for (size_t i = 0; i < frames.size(); ++i) {
    if (frames[i].size() < expected_frame_size) {
      std::cerr << "Error: Frame " << i << " size too small: " << frames[i].size()
      << ", expected " << expected_frame_size << "\n";
      return 1;
    }
  }

  AVFormatContext* fmt_ctx = nullptr;
  if (avformat_alloc_output_context2(&fmt_ctx, nullptr, "mp4", filename) < 0 || !fmt_ctx) {
    std::cerr << "Could not allocate output context\n";
    return 1;
  }

  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!codec) {
    std::cerr << "Codec not found\n";
    avformat_free_context(fmt_ctx);
    return 1;
  }

  AVStream* video_st = avformat_new_stream(fmt_ctx, nullptr);
  if (!video_st) {
    std::cerr << "Could not create stream\n";
    avformat_free_context(fmt_ctx);
    return 1;
  }
  video_st->id = fmt_ctx->nb_streams - 1;

  AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
    std::cerr << "Could not allocate codec context\n";
    avformat_free_context(fmt_ctx);
    return 1;
  }

  codec_ctx->width = width;
  codec_ctx->height = height;
  codec_ctx->time_base = AVRational{1, fps};
  video_st->time_base = codec_ctx->time_base;
  codec_ctx->framerate = AVRational{fps, 1};
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

  // Set color metadata (limited range BT.709)
  codec_ctx->color_range = AVCOL_RANGE_MPEG;  // limited range
  codec_ctx->colorspace = AVCOL_SPC_BT709;
  codec_ctx->color_primaries = AVCOL_PRI_BT709;
  codec_ctx->color_trc = AVCOL_TRC_BT709;

  codec_ctx->gop_size = 12;
  codec_ctx->max_b_frames = 2;

  AVDictionary* codec_options = nullptr;
  av_dict_set(&codec_options, "preset", "medium", 0);

  if (avcodec_open2(codec_ctx, codec, &codec_options) < 0) {
    std::cerr << "Could not open codec\n";
    av_dict_free(&codec_options);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    return 1;
  }
  av_dict_free(&codec_options);

  if (avcodec_parameters_from_context(video_st->codecpar, codec_ctx) < 0) {
    std::cerr << "Failed to copy codec parameters\n";
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    return 1;
  }

  if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
      std::cerr << "Could not open output file\n";
      avcodec_free_context(&codec_ctx);
      avformat_free_context(fmt_ctx);
      return 1;
    }
  }

  if (avformat_write_header(fmt_ctx, nullptr) < 0) {
    std::cerr << "Error occurred when opening output file\n";
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&fmt_ctx->pb);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    return 1;
  }

  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    std::cerr << "Could not allocate frame\n";
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&fmt_ctx->pb);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    return 1;
  }
  frame->format = codec_ctx->pix_fmt;
  frame->width = codec_ctx->width;
  frame->height = codec_ctx->height;

  // Set frame color metadata to match codec context
  frame->color_range = AVCOL_RANGE_MPEG;
  frame->colorspace = AVCOL_SPC_BT709;
  frame->color_primaries = AVCOL_PRI_BT709;
  frame->color_trc = AVCOL_TRC_BT709;

  if (av_frame_get_buffer(frame, 32) < 0) {
    std::cerr << "Could not allocate frame data\n";
    av_frame_free(&frame);
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&fmt_ctx->pb);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    return 1;
  }

  SwsContext* sws_ctx = sws_getContext(
    width, height, AV_PIX_FMT_RGB24,
    width, height, AV_PIX_FMT_YUV420P,
    SWS_BILINEAR, nullptr, nullptr, nullptr);

  if (!sws_ctx) {
    std::cerr << "Could not initialize sws context\n";
    av_frame_free(&frame);
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&fmt_ctx->pb);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    return 1;
  }

  AVPacket* pkt = av_packet_alloc();
  if (!pkt) {
    std::cerr << "Could not allocate packet\n";
    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&fmt_ctx->pb);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    return 1;
  }

  int frame_index = 0;
  for (const auto& rgb_data : frames) {
    const uint8_t* in_data[1] = { rgb_data.data() };
    int in_linesize[1] = { 3 * width };

    sws_scale(sws_ctx, in_data, in_linesize, 0, height, frame->data, frame->linesize);

    frame->pts = frame_index++;

    if (avcodec_send_frame(codec_ctx, frame) < 0) {
      std::cerr << "Error sending frame to encoder\n";
      break;
    }

    while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
      pkt->stream_index = video_st->index;
      pkt->pts = av_rescale_q(pkt->pts, codec_ctx->time_base, video_st->time_base);
      pkt->dts = av_rescale_q(pkt->dts, codec_ctx->time_base, video_st->time_base);

      if (av_interleaved_write_frame(fmt_ctx, pkt) < 0) {
        std::cerr << "Error writing frame\n";
        break;
      }
      av_packet_unref(pkt);
    }
  }

  // Flush encoder
  if (avcodec_send_frame(codec_ctx, nullptr) < 0) {
    std::cerr << "Error sending flush frame\n";
  }

  while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
    pkt->stream_index = video_st->index;
    pkt->pts = av_rescale_q(pkt->pts, codec_ctx->time_base, video_st->time_base);
    pkt->dts = av_rescale_q(pkt->dts, codec_ctx->time_base, video_st->time_base);
    av_interleaved_write_frame(fmt_ctx, pkt);
    av_packet_unref(pkt);
  }

  av_write_trailer(fmt_ctx);

  av_packet_free(&pkt);
  sws_freeContext(sws_ctx);
  av_frame_free(&frame);

  if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&fmt_ctx->pb);
  }

  avcodec_free_context(&codec_ctx);
  avformat_free_context(fmt_ctx);

  std::cout << "Encoding finished, output file: " << filename << "\n";
  return 0;
}

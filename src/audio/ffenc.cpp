#include "ffenc.h"

using namespace audio;
using namespace std::literals;

constexpr auto SAMPLE_RATE = 48000;

#ifdef av_err2str
  #undef av_err2str
  #include <iomanip>
  #include <string>
av_always_inline std::string av_err2string(int errnum) {
  char str[AV_ERROR_MAX_STRING_SIZE];
  return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
  #define av_err2str(err) av_err2string(err).c_str()
#endif

bool
ff_encoder::init(config_t &config) {
  swr = swr_alloc();
  if (!swr) {
    BOOST_LOG(error) << "Could not allocate resampler context"sv;
    return false;
  }

  const AVCodec *codec = avcodec_find_encoder(codec_id);
  if (!codec) {
    BOOST_LOG(error) << avcodec_get_name(codec_id) << " Codec not found"sv;
    return false;
  }
  ctx = avcodec_alloc_context3(codec);
  if (!ctx) {
    BOOST_LOG(error) << "Could not allocate codec context"sv;
    return false;
  }
  ctx->sample_rate = SAMPLE_RATE;
  ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
  ctx->max_samples = samples_per_frame;
  // With this bitrate, each audio frame will be roughly 1280 bytes
  ctx->bit_rate = 320000;
  if (!codec->ch_layouts) {
    av_channel_layout_default(&ctx->ch_layout, config.channels);
  }
  else {
    for (int i = 0; codec->ch_layouts[i].order; i++) {
      if (codec->ch_layouts[i].nb_channels == config.channels) {
        av_channel_layout_copy(&ctx->ch_layout, &codec->ch_layouts[i]);
        break;
      }
    }
  }
  if (avcodec_open2(ctx, codec, nullptr) < 0) {
    avcodec_free_context(&ctx);
    BOOST_LOG(error) << "Failed to open codec"sv;
    return false;
  }
  frame = new_frame(ctx->frame_size, ctx->sample_fmt, &ctx->ch_layout);
  if (!frame) {
    BOOST_LOG(error) << "Could not allocate frame"sv;
    return false;
  }
  frame_s16 = new_frame(ctx->frame_size, AV_SAMPLE_FMT_S16, &ctx->ch_layout);
  if (!frame_s16) {
    BOOST_LOG(error) << "Could not allocate frame"sv;
    return false;
  }
  if (swr_config_frame(swr, frame, frame_s16) < 0) {
    BOOST_LOG(error) << "Could not configure resampler"sv;
    return false;
  }
  if (swr_init(swr) < 0) {
    BOOST_LOG(error) << "Could not initialize resampler"sv;
    return false;
  }
  return true;
}

ff_encoder::~ff_encoder() {
  if (frame_s16) {
    av_frame_free(&frame_s16);
  }
  if (frame) {
    av_frame_free(&frame);
  }
  if (swr) {
    swr_free(&swr);
  }
  if (ctx) {
    avcodec_close(ctx);
    avcodec_free_context(&ctx);
  }
}

bool
ff_encoder::encode(std::vector<std::int16_t> &sample, buffer_t &packet) {
  int nb_channels = ctx->ch_layout.nb_channels;
  if (sample.size() != nb_channels * ctx->frame_size) {
    BOOST_LOG(error) << "Invalid number of samples: " << sample.size() << " != " << nb_channels * ctx->frame_size;
    return false;
  }
  int ret;
  if ((ret = av_frame_make_writable(frame_s16)) < 0) {
    BOOST_LOG(error) << "Could not make frame_s16 writable: "sv << av_err2str(ret);
    return false;
  }
  auto sample_fmt = (AVSampleFormat) frame_s16->format;
  if (avcodec_fill_audio_frame(frame_s16, nb_channels, sample_fmt, (const uint8_t *) sample.data(),
        nb_channels * av_get_bytes_per_sample(sample_fmt) * frame_s16->nb_samples, 0) < 0) {
    BOOST_LOG(error) << "Could not fill audio frame"sv;
    return false;
  }
  if (av_frame_make_writable(frame) < 0) {
    BOOST_LOG(error) << "Could not make frame writable"sv;
    return false;
  }
  if (swr_convert_frame(swr, frame, frame_s16) < 0) {
    BOOST_LOG(error) << "Could not convert frame"sv;
    return false;
  }
  if ((ret = avcodec_send_frame(ctx, frame)) < 0) {
    BOOST_LOG(error) << "Could not send frame: "sv << av_err2str(ret);
    return false;
  }
  AVPacket pkt { nullptr };
  if ((ret = avcodec_receive_packet(ctx, &pkt)) < 0) {
    BOOST_LOG(error) << "Could not receive packet: "sv << av_err2str(ret);
    return false;
  }
  memcpy(std::begin(packet), pkt.data, pkt.size);
  packet.fake_resize(pkt.size);
  return true;
}

AVFrame *
ff_encoder::new_frame(int samples, AVSampleFormat format, const AVChannelLayout *layout) {
  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    return nullptr;
  }
  frame->sample_rate = SAMPLE_RATE;
  frame->nb_samples = samples;
  frame->format = format;
  av_channel_layout_copy(&frame->ch_layout, layout);
  if (av_frame_get_buffer(frame, 0) < 0) {
    delete frame;
    return nullptr;
  }
  return frame;
}

ff_encoder::ff_encoder(AVCodecID codec_id, int samples_per_frame):
    codec_id(codec_id), samples_per_frame(samples_per_frame) {
}

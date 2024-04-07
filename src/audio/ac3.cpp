#include <bitset>

#include "../audio.h"
#include "../config.h"
#include "../logging.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

using namespace audio;
using namespace std::literals;

constexpr auto SAMPLE_RATE = 48000;

constexpr int channel_map[6] = { 0, 1, 2, 3, 4, 5 };

class ac3_encoder: public audio_encoder {
public:
  ~ac3_encoder() override;
  bool
  init(config_t &config) override;
  bool
  encode(std::vector<std::int16_t> &sample, buffer_t &packet) override;

private:
  SwrContext *swr = nullptr;
  AVCodecContext *ctx = nullptr;
  AVFrame *frame = nullptr;
  AVFrame *frame_s16 = nullptr;
  AVPacket *pkt = nullptr;

  static AVFrame *
  new_frame(int samples, AVSampleFormat format, const AVChannelLayout *layout);
};

bool
ac3_encoder::init(config_t &config) {
  swr = swr_alloc();
  if (!swr) {
    BOOST_LOG(error) << "Could not allocate resampler context"sv;
    return false;
  }
  const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AC3);
  if (!codec) {
    BOOST_LOG(error) << "AC3 Codec not found"sv;
    return false;
  }
  ctx = avcodec_alloc_context3(codec);
  if (!ctx) {
    BOOST_LOG(error) << "Could not allocate codec context"sv;
    return false;
  }
  ctx->sample_rate = SAMPLE_RATE;
  ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
  ctx->max_samples = 1536;
  // With this bitrate, each audio frame will be roughly 1280 bytes
  ctx->bit_rate = 320000;
  for (int i = 0; codec->ch_layouts[i].order; i++) {
    if (codec->ch_layouts[i].nb_channels == config.channels) {
      av_channel_layout_copy(&ctx->ch_layout, &codec->ch_layouts[i]);
      break;
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
  if (swr_set_channel_mapping(swr, channel_map) < 0) {
    BOOST_LOG(error) << "Could not configure resampler"sv;
    return false;
  }
  if (swr_init(swr) < 0) {
    BOOST_LOG(error) << "Could not initialize resampler"sv;
    return false;
  }
  pkt = av_packet_alloc();
  if (!pkt) {
    BOOST_LOG(error) << "Could not allocate packet"sv;
    return false;
  }
  return true;
}

ac3_encoder::~ac3_encoder() {
  if (frame_s16) {
    av_frame_free(&frame_s16);
  }
  if (frame) {
    av_frame_free(&frame);
  }
  if (pkt) {
    av_packet_free(&pkt);
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
ac3_encoder::encode(std::vector<std::int16_t> &sample, buffer_t &packet) {
  int nb_channels = ctx->ch_layout.nb_channels;
  if (sample.size() != nb_channels * ctx->frame_size) {
    BOOST_LOG(error) << "Invalid number of samples"sv;
    return false;
  }
  if (av_frame_make_writable(frame_s16) < 0) {
    BOOST_LOG(error) << "Could not make frame_s16 writable"sv;
    return false;
  }
  auto sample_fmt = (AVSampleFormat) frame_s16->format;
  if (avcodec_fill_audio_frame(frame_s16, nb_channels, sample_fmt, (const uint8_t *) sample.data(),
        nb_channels * av_get_bytes_per_sample(sample_fmt) * frame_s16->nb_samples, 0) < 0) {
    BOOST_LOG(error) << "Could not fill audio frame"sv;
  }
  if (av_frame_make_writable(frame) < 0) {
    BOOST_LOG(error) << "Could not make frame writable"sv;
    return false;
  }
  if (swr_convert_frame(swr, frame, frame_s16) < 0) {
    BOOST_LOG(error) << "Could not convert frame"sv;
    return false;
  }
  if (avcodec_send_frame(ctx, frame) < 0) {
    BOOST_LOG(error) << "Could not send frame"sv;
    return false;
  }
  if (avcodec_receive_packet(ctx, pkt) < 0) {
    BOOST_LOG(error) << "Could not receive packet"sv;
    return false;
  }
  memcpy(std::begin(packet), pkt->data, pkt->size);
  packet.fake_resize(pkt->size);
  av_packet_unref(pkt);
  return true;
}

AVFrame *
ac3_encoder::new_frame(int samples, AVSampleFormat format, const AVChannelLayout *layout) {
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

audio_encoder *
audio_encoder::create_ac3() {
  return new ac3_encoder();
}
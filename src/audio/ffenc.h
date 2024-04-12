#include <bitset>

#include "../audio.h"
#include "../config.h"
#include "../logging.h"

#pragma once
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

namespace audio {

  class ff_encoder: public audio_encoder {
  public:
    ~ff_encoder() override;
    bool
    init(config_t &config) override;
    bool
    encode(std::vector<std::int16_t> &sample, buffer_t &packet) override;

  protected:
    explicit ff_encoder(AVCodecID codec_id, int samples_per_frame);
    AVCodecID codec_id;
    int samples_per_frame;
  private:
    SwrContext *swr = nullptr;
    AVCodecContext *ctx = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *frame_s16 = nullptr;

    static AVFrame *
    new_frame(int samples, AVSampleFormat format, const AVChannelLayout *layout);
  };

}
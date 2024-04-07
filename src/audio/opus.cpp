#include <bitset>
#include <opus/opus_multistream.h>

#include "../audio.h"
#include "../config.h"
#include "../logging.h"

using namespace audio;
using namespace std::literals;

class opus_encoder: public audio_encoder {
  using opus_t = util::safe_ptr<OpusMSEncoder, opus_multistream_encoder_destroy>;

public:
  ~opus_encoder() override;
  bool
  init(config_t &config) override;
  bool
  encode(std::vector<std::int16_t> &sample, buffer_t &packet) override;

protected:
  opus_t opus;
  int frame_size;
};

bool
opus_encoder::init(config_t &config) {
  auto stream = &stream_configs[map_stream(config.channels, config.flags[config_t::HIGH_QUALITY])];
  opus = opus_t { opus_multistream_encoder_create(
    stream->sampleRate,
    stream->channelCount,
    stream->streams,
    stream->coupledStreams,
    stream->mapping,
    OPUS_APPLICATION_RESTRICTED_LOWDELAY,
    nullptr) };
  opus_multistream_encoder_ctl(opus.get(), OPUS_SET_BITRATE(stream->bitrate));
  opus_multistream_encoder_ctl(opus.get(), OPUS_SET_VBR(0));
  frame_size = config.packetDuration * stream->sampleRate / 1000;
  return true;
}

opus_encoder::~opus_encoder() {
  opus.reset();
}

bool
opus_encoder::encode(std::vector<std::int16_t> &sample, buffer_t &packet) {
  int bytes = opus_multistream_encode(opus.get(), sample.data(), frame_size, std::begin(packet), packet.size());
  if (bytes < 0) {
    BOOST_LOG(error) << "Couldn't encode audio: "sv << opus_strerror(bytes);
    return false;
  }

  packet.fake_resize(bytes);
  return true;
}

audio_encoder *
audio_encoder::create_opus() {
  return new opus_encoder();
}
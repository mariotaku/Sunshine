/**
 * @file src/audio.h
 * @brief todo
 */
#pragma once

#include "thread_safe.h"
#include "utility.h"
namespace audio {
  enum stream_config_e : int {
    STEREO,
    HIGH_STEREO,
    SURROUND51,
    HIGH_SURROUND51,
    SURROUND71,
    HIGH_SURROUND71,
    MAX_STREAM_CONFIG
  };

  struct opus_stream_config_t {
    std::int32_t sampleRate;
    int channelCount;
    int streams;
    int coupledStreams;
    const std::uint8_t *mapping;
    int bitrate;
  };

  extern opus_stream_config_t stream_configs[MAX_STREAM_CONFIG];

  int
  map_stream(int channels, bool quality);

  struct config_t {
    enum flags_e : int {
      HIGH_QUALITY,
      HOST_AUDIO,
      MAX_FLAGS
    };

    double packetDuration;
    int channels;
    int mask;

    int audioFormat; // AUDIO_FORMAT_* from Limelight.h

    std::bitset<MAX_FLAGS> flags;
  };

  using buffer_t = util::buffer_t<std::uint8_t>;
  using packet_t = std::pair<void *, buffer_t>;
  void
  capture(safe::mail_t mail, config_t config, void *channel_data);

  class audio_encoder {
  public:
    virtual ~audio_encoder() = default;
    virtual bool init(config_t &config) = 0;
    virtual bool encode(std::vector<std::int16_t> &sample, buffer_t &packet) = 0;

    static audio_encoder *create(config_t &config);
  protected:
    static audio_encoder *
    create_opus();
    static audio_encoder *
    create_ac3();
    static audio_encoder *
    create_eac3();
  };


}  // namespace audio

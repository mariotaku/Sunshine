#include "ffenc.h"

using namespace audio;
using namespace std::literals;

class eac3_encoder: public ff_encoder {
public:
  eac3_encoder():
      ff_encoder(AV_CODEC_ID_AAC, 256) {};
};

audio_encoder *
audio_encoder::create_eac3() {
  return new eac3_encoder();
}
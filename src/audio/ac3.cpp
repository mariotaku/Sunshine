#include "ffenc.h"

using namespace audio;
using namespace std::literals;

class ac3_encoder: public ff_encoder {
public:
  ac3_encoder():
      ff_encoder(AV_CODEC_ID_AC3, 320000, 1536) {};
};

audio_encoder *
audio_encoder::create_ac3() {
  return new ac3_encoder();
}
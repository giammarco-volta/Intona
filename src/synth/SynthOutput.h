#pragma once
#include "AdditiveSynth.h"
#include <QString>
#include <memory>

namespace Intona::Synth {
// Device lifetime encloses every render call. Open/close only on the control thread.
class SynthOutput {
public:
  SynthOutput();
  ~SynthOutput();
  QString start();
  QString deviceError() const;
  AdditiveSynth engine{48000};
private:
  struct Device;
  std::unique_ptr<Device> device_;
};
}

#include "SynthOutput.h"

#ifdef Q_OS_ANDROID
#include <aaudio/AAudio.h>
#endif

namespace Intona::Synth {
struct SynthOutput::Device {
  std::atomic<int> error{0};
#ifdef Q_OS_ANDROID
  AAudioStream* stream = nullptr;
  ~Device() {
    if (stream) {
      AAudioStream_requestStop(stream);
      AAudioStream_close(stream);
    }
  }
#endif
};

SynthOutput::SynthOutput() : device_(std::make_unique<Device>()) {}
SynthOutput::~SynthOutput() = default;

QString SynthOutput::deviceError() const {
  const int error = device_->error.load();
  if (!error) return {};
#ifdef Q_OS_ANDROID
  return QString::fromLatin1(AAudio_convertResultToText(error));
#else
  return {};
#endif
}

QString SynthOutput::start() {
#ifdef Q_OS_ANDROID
  AAudioStreamBuilder* builder = nullptr;
  auto status = AAudio_createStreamBuilder(&builder);
  if (status != AAUDIO_OK) return QString::fromLatin1(AAudio_convertResultToText(status));
  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
  AAudioStreamBuilder_setChannelCount(builder, 1);
  AAudioStreamBuilder_setSampleRate(builder, 48000);
  AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setDataCallback(builder, [](AAudioStream*, void* user, void* pcm, int32_t frames) -> aaudio_data_callback_result_t {
    static_cast<AdditiveSynth*>(user)->render(static_cast<float*>(pcm), frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }, &engine);
  AAudioStreamBuilder_setErrorCallback(builder, [](AAudioStream*, void* user, aaudio_result_t error) {
    static_cast<Device*>(user)->error.store(error);
  }, device_.get());
  status = AAudioStreamBuilder_openStream(builder, &device_->stream);
  AAudioStreamBuilder_delete(builder);
  if (status == AAUDIO_OK) status = AAudioStream_requestStart(device_->stream);
  return status == AAUDIO_OK ? QString{} : QString::fromLatin1(AAudio_convertResultToText(status));
#else
  return QStringLiteral("Internal audio is available on Windows and Android.");
#endif
}
}

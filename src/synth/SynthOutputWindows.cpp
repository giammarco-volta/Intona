#include "SynthOutput.h"

#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <future>
#include <thread>
#include <system_error>

namespace Intona::Synth {
namespace {
using Microsoft::WRL::ComPtr;

struct Stream {
  ComPtr<IAudioClient> client;
  ComPtr<IAudioRenderClient> render;
  UINT32 capacity = 0;

  HRESULT open(HANDLE wake) {
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) return hr;
    ComPtr<IMMDevice> endpoint;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &endpoint);
    if (FAILED(hr)) return hr;
    hr = endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(client.GetAddressOf()));
    if (FAILED(hr)) return hr;
    // The shared engine converts this stereo stream to the device mix format.
    WAVEFORMATEX format{WAVE_FORMAT_IEEE_FLOAT, 2, 48000, 48000 * 8, 8, 32, 0};
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
      AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 300000, 0, &format, nullptr);
    if (FAILED(hr)) return hr;
    hr = client->SetEventHandle(wake);
    if (FAILED(hr)) return hr;
    hr = client->GetBufferSize(&capacity);
    if (FAILED(hr)) return hr;
    hr = client->GetService(IID_PPV_ARGS(&render));
    if (FAILED(hr)) return hr;
    BYTE* data;
    hr = render->GetBuffer(capacity, &data);
    if (FAILED(hr)) return hr;
    hr = render->ReleaseBuffer(capacity, AUDCLNT_BUFFERFLAGS_SILENT);
    return FAILED(hr) ? hr : client->Start();
  }

  HRESULT refill(AdditiveSynth& engine) {
    UINT32 queued;
    HRESULT hr = client->GetCurrentPadding(&queued);
    if (FAILED(hr) || queued == capacity) return hr;
    const UINT32 frames = capacity - queued;
    BYTE* data;
    hr = render->GetBuffer(frames, &data);
    if (FAILED(hr)) return hr;
    float* stereo = reinterpret_cast<float*>(data);
    std::array<float, 256> mono;
    for (UINT32 pos = 0; pos < frames;) {
      const UINT32 count = std::min<UINT32>(UINT32(mono.size()), frames - pos);
      engine.render(mono.data(), int(count));
      for (UINT32 i = 0; i < count; ++i) {
        *stereo++ = mono[i];
        *stereo++ = mono[i];
      }
      pos += count;
    }
    return render->ReleaseBuffer(frames, 0);
  }
};

QString errorText(HRESULT hr) {
  if (SUCCEEDED(hr)) return {};
  return QStringLiteral("Windows audio: %1 (0x%2)")
    .arg(QString::fromStdString(std::system_category().message(hr)))
    .arg(quint32(hr), 8, 16, QLatin1Char('0'));
}
}

struct SynthOutput::Device {
  HANDLE stop = nullptr, wake = nullptr;
  std::thread thread;
  std::atomic<HRESULT> error{S_OK};
  ~Device() {
    if (stop) SetEvent(stop);
    if (thread.joinable()) thread.join();
    if (wake) CloseHandle(wake);
    if (stop) CloseHandle(stop);
  }
  void run(AdditiveSynth& engine, std::promise<HRESULT>& ready) {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init)) { ready.set_value(init); return; }
    {
      Stream stream;
      HRESULT hr = stream.open(wake);
      ready.set_value(hr);
      if (SUCCEEDED(hr)) {
        DWORD index = 0;
        HANDLE priority = AvSetMmThreadCharacteristicsW(L"Audio", &index);
        HANDLE events[] = {stop, wake};
        while (SUCCEEDED(hr)) {
          const DWORD event = WaitForMultipleObjects(2, events, FALSE, INFINITE);
          if (event == WAIT_OBJECT_0) break;
          hr = event == WAIT_OBJECT_0 + 1 ? stream.refill(engine) : HRESULT_FROM_WIN32(GetLastError());
        }
        if (priority) AvRevertMmThreadCharacteristics(priority);
        stream.client->Stop();
        error.store(hr);
      }
    }
    CoUninitialize();
  }
};

SynthOutput::SynthOutput() : device_(std::make_unique<Device>()) {}
SynthOutput::~SynthOutput() = default;
QString SynthOutput::deviceError() const { return errorText(device_->error.load()); }
QString SynthOutput::start() {
  auto& d = *device_;
  d.stop = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  if (!d.stop) return errorText(HRESULT_FROM_WIN32(GetLastError()));
  d.wake = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!d.wake) return errorText(HRESULT_FROM_WIN32(GetLastError()));
  std::promise<HRESULT> ready;
  auto started = ready.get_future();
  try {
    d.thread = std::thread([this, p = std::move(ready)]() mutable { device_->run(engine, p); });
  } catch (const std::system_error& error) {
    return QString::fromLocal8Bit(error.what());
  }
  return errorText(started.get());
}
} // namespace Intona::Synth

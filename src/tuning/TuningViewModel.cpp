#include "TuningViewModel.h"

#include <QMetaObject>

namespace Intona::Tuning
{

TuningViewModel::TuningViewModel(
  TuningController* worker,
  QObject* parent)
  : QObject(parent),
    worker_(worker),
    state_(worker->uiSnapshot())
{
  connect(
    worker_,
    &TuningController::tuningStateChanged,
    this,
    &TuningViewModel::requestRefreshFromWorker,
    Qt::DirectConnection);
}

void TuningViewModel::setEdoIndex(int index)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, index]() { worker->setEdoIndex(index); },
    Qt::QueuedConnection);
}

void TuningViewModel::selectTuningCenter(int value)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, value]() { worker->selectTuningCenter(value); },
    Qt::QueuedConnection);
}

void TuningViewModel::stepKeyPitch(int keyIndex, int direction)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, keyIndex, direction]()
    {
      worker->stepKeyPitch(keyIndex, direction);
    },
    Qt::QueuedConnection);
}

void TuningViewModel::captureCurrentPreset()
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_]() { worker->captureCurrentPreset(); },
    Qt::QueuedConnection);
}

void TuningViewModel::applyPreset(int index)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, index]() { worker->applyPreset(index); },
    Qt::QueuedConnection);
}

void TuningViewModel::deletePreset(int index)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, index]() { worker->deletePreset(index); },
    Qt::QueuedConnection);
}

void TuningViewModel::setAdaptingEnabled(bool enabled)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, enabled]() { worker->setAdaptingEnabled(enabled); },
    Qt::QueuedConnection);
}

void TuningViewModel::cycleAftertouchMode()
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_]() { worker->cycleAftertouchMode(); },
    Qt::QueuedConnection);
}

void TuningViewModel::requestRefreshFromWorker()
{
  requestedRevision_.fetch_add(1, std::memory_order_relaxed);
  postRefreshIfNeeded();
}

void TuningViewModel::requestInitialRefresh()
{
  requestRefreshFromWorker();
}

void TuningViewModel::postRefreshIfNeeded()
{
  bool expected = false;
  if (!refreshPosted_.compare_exchange_strong(
        expected,
        true,
        std::memory_order_acq_rel))
  {
    return;
  }

  QMetaObject::invokeMethod(
    this,
    [this]() { refreshFromWorker(); },
    Qt::QueuedConnection);
}

void TuningViewModel::refreshFromWorker()
{
  const uint64_t revision =
    requestedRevision_.load(std::memory_order_acquire);

  TuningUiSnapshot snapshot;
  const bool read = QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, &snapshot]()
    {
      snapshot = worker->uiSnapshot();
    },
    Qt::BlockingQueuedConnection);

  if (read)
  {
    state_ = std::move(snapshot);
    emit tuningStateChanged();
  }

  refreshPosted_.store(false, std::memory_order_release);

  if (requestedRevision_.load(std::memory_order_acquire) != revision)
    postRefreshIfNeeded();
}

} // namespace Intona::Tuning

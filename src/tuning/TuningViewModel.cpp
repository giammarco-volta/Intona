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


void TuningViewModel::setRetriggerHeldNotes(bool enabled)
{
  QMetaObject::invokeMethod(worker_, [worker = worker_, enabled]() {
    worker->setRetriggerHeldNotes(enabled);
  }, Qt::QueuedConnection);
}

void TuningViewModel::startRetuningTest()
{
  QMetaObject::invokeMethod(worker_, &TuningController::startRetuningTest, Qt::QueuedConnection);
}

void TuningViewModel::cancelRetuningTest()
{
  QMetaObject::invokeMethod(worker_, &TuningController::cancelRetuningTest, Qt::QueuedConnection);
}

void TuningViewModel::setNoteNamingMode(int mode)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, mode]() { worker->setNoteNamingMode(mode); },
    Qt::QueuedConnection);
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

void TuningViewModel::moveKeyPitchBySteps(
  int keyIndex,
  int stepCount)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, keyIndex, stepCount]()
    {
      worker->moveKeyPitchBySteps(keyIndex, stepCount);
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

void TuningViewModel::setControlSource(int source)
{
  QMetaObject::invokeMethod(worker_, [worker = worker_, source]() { worker->setControlSource(source); }, Qt::QueuedConnection);
}

void TuningViewModel::setControlAction(int action)
{
  QMetaObject::invokeMethod(worker_, [worker = worker_, action]() { worker->setControlAction(action); }, Qt::QueuedConnection);
}

void TuningViewModel::setControlThreshold(int threshold)
{
  QMetaObject::invokeMethod(worker_, [worker = worker_, threshold]() { worker->setControlThreshold(threshold); }, Qt::QueuedConnection);
}

void TuningViewModel::setControlEnabled(bool enabled)
{
  QMetaObject::invokeMethod(worker_, [worker = worker_, enabled]() { worker->setControlEnabled(enabled); }, Qt::QueuedConnection);
}

void TuningViewModel::toggleControlDirection()
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_]() { worker->toggleControlDirection(); },
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
    const bool circleChanged = state_.circleEntries != snapshot.circleEntries;
    const bool pressedChanged = state_.pressedKeys != snapshot.pressedKeys;
    state_ = std::move(snapshot);
    if (circleChanged)
      emit circleEntriesChanged();
    if (pressedChanged)
      emit pressedKeysChanged();
    emit tuningStateChanged();
  }

  refreshPosted_.store(false, std::memory_order_release);

  if (requestedRevision_.load(std::memory_order_acquire) != revision)
    postRefreshIfNeeded();
}

} // namespace Intona::Tuning

#pragma once

#include "TuningController.h"

#include <QObject>
#include <atomic>

namespace Intona::Tuning
{

class TuningViewModel final : public QObject
{
  Q_OBJECT

  Q_PROPERTY(int edoIndex READ edoIndex WRITE setEdoIndex NOTIFY tuningStateChanged)
  Q_PROPERTY(int edo READ edo NOTIFY tuningStateChanged)
  Q_PROPERTY(QVariantList availableEdos READ availableEdos CONSTANT)
  Q_PROPERTY(int tuningCenter READ tuningCenter NOTIFY tuningStateChanged)
  Q_PROPERTY(QString tuningCenterName READ tuningCenterName NOTIFY tuningStateChanged)
  Q_PROPERTY(QVariantList keyValues READ keyValues NOTIFY tuningStateChanged)
  Q_PROPERTY(QStringList keyNames READ keyNames NOTIFY tuningStateChanged)
  Q_PROPERTY(QVariantList canRaiseKeys READ canRaiseKeys NOTIFY tuningStateChanged)
  Q_PROPERTY(QVariantList canLowerKeys READ canLowerKeys NOTIFY tuningStateChanged)
  Q_PROPERTY(QVariantList circleEntries READ circleEntries NOTIFY tuningStateChanged)
  Q_PROPERTY(QVariantList presetEntries READ presetEntries NOTIFY tuningStateChanged)
  Q_PROPERTY(int currentPresetIndex READ currentPresetIndex NOTIFY tuningStateChanged)
  Q_PROPERTY(bool adaptingEnabled READ adaptingEnabled WRITE setAdaptingEnabled NOTIFY tuningStateChanged)
  Q_PROPERTY(QString aftertouchText READ aftertouchText NOTIFY tuningStateChanged)
  Q_PROPERTY(bool aftertouchEnabled READ aftertouchEnabled NOTIFY tuningStateChanged)
  Q_PROPERTY(QString keyDescription READ keyDescription NOTIFY tuningStateChanged)
  Q_PROPERTY(QString chordDescription READ chordDescription NOTIFY tuningStateChanged)
  Q_PROPERTY(QVariantList pressedKeys READ pressedKeys NOTIFY tuningStateChanged)

public:
  explicit TuningViewModel(
    TuningController* worker,
    QObject* parent = nullptr);

  int edoIndex() const { return state_.edoIndex; }
  int edo() const { return state_.edo; }
  QVariantList availableEdos() const { return state_.availableEdos; }
  int tuningCenter() const { return state_.tuningCenter; }
  QString tuningCenterName() const { return state_.tuningCenterName; }
  QVariantList keyValues() const { return state_.keyValues; }
  QStringList keyNames() const { return state_.keyNames; }
  QVariantList canRaiseKeys() const { return state_.canRaiseKeys; }
  QVariantList canLowerKeys() const { return state_.canLowerKeys; }
  QVariantList circleEntries() const { return state_.circleEntries; }
  QVariantList presetEntries() const { return state_.presetEntries; }
  int currentPresetIndex() const { return state_.currentPresetIndex; }
  bool adaptingEnabled() const { return state_.adaptingEnabled; }
  QString aftertouchText() const { return state_.aftertouchText; }
  bool aftertouchEnabled() const { return state_.aftertouchEnabled; }
  QString keyDescription() const { return state_.keyDescription; }
  QString chordDescription() const { return state_.chordDescription; }
  QVariantList pressedKeys() const { return state_.pressedKeys; }

  void setEdoIndex(int index);
  Q_INVOKABLE void selectTuningCenter(int value);
  Q_INVOKABLE void stepKeyPitch(int keyIndex, int direction);
  Q_INVOKABLE void captureCurrentPreset();
  Q_INVOKABLE void applyPreset(int index);
  Q_INVOKABLE void deletePreset(int index);
  void setAdaptingEnabled(bool enabled);
  Q_INVOKABLE void cycleAftertouchMode();

  void requestRefreshFromWorker();
  void requestInitialRefresh();

signals:
  void tuningStateChanged();

private:
  void postRefreshIfNeeded();
  void refreshFromWorker();

  TuningController* worker_ = nullptr;
  TuningUiSnapshot state_;
  std::atomic_uint64_t requestedRevision_{0};
  std::atomic_bool refreshPosted_{false};
};

} // namespace Intona::Tuning

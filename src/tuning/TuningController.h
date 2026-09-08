#pragma once

#include "../Config.hpp"
#include "TuningTypes.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace Intona::Tuning
{

class TuningController final : public QObject
{
  Q_OBJECT

  Q_PROPERTY(int edoIndex
             READ edoIndex
             WRITE setEdoIndex
             NOTIFY tuningStateChanged)

  Q_PROPERTY(int edo
             READ edo
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList availableEdos
             READ availableEdos
             CONSTANT)

  Q_PROPERTY(int tuningCenter
             READ tuningCenter
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QString tuningCenterName
             READ tuningCenterName
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList keyValues
             READ keyValues
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QStringList keyNames
             READ keyNames
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList canRaiseKeys
             READ canRaiseKeys
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList canLowerKeys
             READ canLowerKeys
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList circleEntries
             READ circleEntries
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList presetEntries
             READ presetEntries
             NOTIFY tuningStateChanged)

  Q_PROPERTY(int currentPresetIndex
             READ currentPresetIndex
             NOTIFY tuningStateChanged)

public:
  explicit TuningController(QObject* parent = nullptr);

  int edoIndex() const;
  void setEdoIndex(int index);

  int edo() const;

  QVariantList availableEdos() const;

  int tuningCenter() const;
  QString tuningCenterName() const;

  Q_INVOKABLE void selectTuningCenter(int value);

  QVariantList keyValues() const;
  QStringList keyNames() const;
  QVariantList canRaiseKeys() const;
  QVariantList canLowerKeys() const;

  Q_INVOKABLE void stepKeyPitch(
    int keyIndex,
    int direction);
  
  QVariantList circleEntries() const;
  QVariantList presetEntries() const;
  int currentPresetIndex() const;

  Q_INVOKABLE void captureCurrentPreset();
  Q_INVOKABLE void applyPreset(int index);
  Q_INVOKABLE void deletePreset(int index);

signals:
  void tuningStateChanged();

private:
  int edoIndex_ = 10;
  Config currentConfig_;
  double currentGlobalOffsetCents_ = 0.0;
  int currentPresetIndex_ = -1;

  std::vector<TuningPreset> loadPresets() const;
  void savePresets(
    const std::vector<TuningPreset>& presets) const;
};

} // namespace Intona::Tuning

#pragma once

#include <QWidget>
#include <QRectF>
#include <QPaintEvent>
#include <QMouseEvent>
#include <vector>

#include "../../Config.hpp"


//-------------------------------------------
class ConfigPresetListWidget : public QWidget
//-------------------------------------------
{
  Q_OBJECT

public:
  explicit ConfigPresetListWidget(QWidget* parent = nullptr);

  void setMapping(const NtetMapping* mapping);
  void setPresets(const std::vector<std::array<int8_t, 12>>& presets);
  void setCurrentPresetIndex(int index);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  bool event(QEvent* event) override;

private:
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  bool handlePressAt(const QPointF& pos);

signals:
  void presetSelected(int index);
  void presetDeleteRequested(int index, int current);

private:
  struct PresetHit
  {
    QRectF bodyRect;
    QRectF deleteRect;
    int index = -1;
  };

  std::vector<PresetHit> hits_;

  const NtetMapping* mapping_ = nullptr;
  const std::vector<std::array<int8_t, 12>>* presets_ = nullptr;

  int currentPresetIndex_ = -1;
};
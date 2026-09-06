#pragma once

#include <QWidget>

class NtetCircleWidget;
class ConfigPresetListWidget;

class SurfaceTab : public QWidget
{
  Q_OBJECT

public:
  explicit SurfaceTab(QWidget* parent = nullptr);

  NtetCircleWidget* getNtetCircleWidget() const { return ntetCircleWidget_; }
  ConfigPresetListWidget* getConfigPresetListWidget() const { return configPresetListWidget_; }

private:
  NtetCircleWidget* ntetCircleWidget_{};
  ConfigPresetListWidget* configPresetListWidget_{};
};
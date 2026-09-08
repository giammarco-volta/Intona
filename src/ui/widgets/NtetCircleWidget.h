#pragma once

#include <QWidget>
#include <QRectF>
#include <QPixmap>
#include <QEvent>

#include <cstdint>
#include <vector>

#include "../../Config.hpp"


//-------------------------------------
class NtetCircleWidget : public QWidget
//-------------------------------------
{
  Q_OBJECT

public:
  explicit NtetCircleWidget(QWidget* parent = nullptr);

  void setCurrentMapping(const NtetMapping* mapping);
  void setConfig(const Config& config);

  void setPressedMask(ConfigMask mask);

  void setAdaptingEnabled(bool enabled);

  void setKeyStepButtonEnabled(const std::array<bool, 12>& canRaise, const std::array<bool, 12>& canLower);

  void setKey(int8_t value, bool isMinor);
  void setChord(int8_t root, QString chordName);
  void setChordRoot(int8_t value);
  void setAfterTouchBehaviourText(const QString& text, bool enabled);

private:
  void drawKeyboardLabels(QPainter& p, const QRectF& r);
  bool event(QEvent* event) override;
  bool handlePressAt(const QPointF& pos);
  void drawKeyboardStepButtons(QPainter& p, const QRectF& keyboardRect);
  void drawLeftStatusLabels(QPainter& p, const QPointF& center, double labelRadius, double outerRadius, const QFont& baseFont);
  void drawCapturePresetLabel(QPainter& p, const QPointF& center, const double labelRadius, const double outerRadius, const QFont& baseFont);
  void rebuildCircleLabels();

signals:
  void tuningCenterSelected(int8_t value);
  void keyPitchRaiseRequested(int keyIndex);
  void keyPitchLowerRequested(int keyIndex);
  void adaptingEnabledToggled();
  void edoLabelClicked();
  void capturePresetRequested();
  void afterTouchBehaviourClicked();

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

private:
  struct LabelHit
  {
    QRectF rect;
    int value;
  };

  struct CircleLabel
  {
    int8_t value5;
    uint8_t pitchStep;
    QString name;
  };

  std::vector<LabelHit> hitAreas_;

  struct ButtonHit
  {
    QRectF rect;
    int keyIndex = -1;
    int delta = 0; // +1 oppure -1
  };

  std::vector<ButtonHit> keyButtonHits_;

  const NtetMapping* mapping_ = nullptr;
  const Config* config_;

  ConfigMask pressedMask5_ = 0;

  QPixmap keyboardPixmap_;

  bool adaptingEnabled_ = true;
  QRectF adaptingToggleRect_;

  QRectF edoLabelRect_;
  QRectF capturePresetRect_;

  QRectF afterTouchRect_;
  QString afterTouchBehaviourText_ = QStringLiteral("Aftertouch: off");
  bool afterTouchEnabled_ = true;

  int8_t keyTonic_ = Config::invalid;
  bool isMinor_ = false;

  int8_t chordRoot_ = Config::invalid;
  QString chordName_;

  std::vector<CircleLabel> labels_;

  std::array<bool, 12> canRaiseKey_{};
  std::array<bool, 12> canLowerKey_{};
};
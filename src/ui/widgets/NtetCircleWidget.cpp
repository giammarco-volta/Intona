#include "NtetCircleWidget.h"

#include <QMouseEvent>
#include <QPaintEvent>

#include <QPainter>
#include <QFontMetricsF>
#include <QtMath>
#include <QDebug>
#include <QFile>
#include <QImageReader>

#include <cmath>

#include "../../StringUtilities.hpp"

namespace
{
  static const QColor kColorBackground{ 20, 20, 20 };

  static const QColor kColorTextPrimary{ 230, 230, 230 };
  static const QColor kColorTextSecondary{ 170, 170, 170 };

  static const QColor kColorDisabled{ 120, 120, 120 };

  static const QColor kColorDisabledStepButtons{ 90, 90, 90 };
  static const QColor kColorCircle{ 140, 140, 140 };

  static const QColor kColorCents{ 95, 95, 95 };

  static const QColor kColorHighlightGreen{ 80, 220, 120 };
  static const QColor kColorHighlightDarkGreen{ 40, 180, 80 };

  static const QColor kColorHighlightCyan{ 80, 220, 235 };

  static const QColor kColorHighlightGold{ 255, 210, 90 };
  static const QColor kColorHighlightDarkGold{ 212, 175, 55 };

  static const QColor kColorHighlightRed{ 240, 80, 80 };
}

//-------------------------------------------------------------------------------------------------------------
NtetCircleWidget::NtetCircleWidget(QWidget* parent) : QWidget(parent), keyboardPixmap_(":/images/keyboard.png")
//-------------------------------------------------------------------------------------------------------------
{
  setMinimumSize(200, 200);

  setAttribute(Qt::WA_AcceptTouchEvents, true);
}

//------------------------------------------------------------------
void NtetCircleWidget::setCurrentMapping(const NtetMapping* mapping)
//------------------------------------------------------------------
{
  mapping_ = mapping;
  update();
}

//----------------------------------------------------
void NtetCircleWidget::setConfig(const Config& config)
//----------------------------------------------------
{
  config_ = &config;
  rebuildCircleLabels();
  update();
}

//----------------------------------------------------
void NtetCircleWidget::setPressedMask(ConfigMask mask)
//----------------------------------------------------
{
  pressedMask5_ = mask;
  update();
}

//-----------------------------------------------------
void NtetCircleWidget::setAdaptingEnabled(bool enabled)
//-----------------------------------------------------
{
  adaptingEnabled_ = enabled;
  update();
}

//------------------------------------------------------------------------------------------------------------------------
void NtetCircleWidget::setKeyStepButtonEnabled(const std::array<bool, 12>& canRaise, const std::array<bool, 12>& canLower)
//------------------------------------------------------------------------------------------------------------------------
{
  canRaiseKey_ = canRaise;
  canLowerKey_ = canLower;
  update();
}

//-------------------------------------------------------
void NtetCircleWidget::setKey(int8_t value, bool isMinor)
//-------------------------------------------------------
{
  keyTonic_ = value;
  isMinor_ = isMinor;
  update();
}

//-------------------------------------------------------------
void NtetCircleWidget::setChord(int8_t root, QString chordName)
//-------------------------------------------------------------
{
  chordName_ = chordName;
  chordRoot_ = root;
  update();
}

//-----------------------------------------------
void NtetCircleWidget::setChordRoot(int8_t value)
//-----------------------------------------------
{
  chordRoot_ = value;
  chordName_.clear();
  update();
}

//----------------------------------------------------------------------------------
void NtetCircleWidget::setAfterTouchBehaviourText(const QString& text, bool enabled)
//----------------------------------------------------------------------------------
{
  afterTouchBehaviourText_ = text;
  afterTouchEnabled_ = enabled;
  update();
}

//----------------------------------------------------------------------------
std::optional<int8_t> NtetCircleWidget::valueForPitchStep(int pitchStep) const
//----------------------------------------------------------------------------
{
  if (!mapping_)
    return std::nullopt;

  pitchStep = mod(pitchStep, mapping_->N);

  for (const auto& label : labels_)
  {
    if (label.pitchStep == pitchStep)
      return static_cast<int8_t>(label.value5);
  }

  return std::nullopt;
}

//-----------------------------------------
bool NtetCircleWidget::event(QEvent* event)
//-----------------------------------------
{
  if (event->type() == QEvent::TouchBegin ||
    event->type() == QEvent::TouchEnd)
  {
    auto* touchEvent = static_cast<QTouchEvent*>(event);

    if (!touchEvent->points().isEmpty())
    {
      const QPointF pos =
        touchEvent->points().first().position();

      if (handlePressAt(pos))
      {
        event->accept();
        return true;
      }
    }
  }

  return QWidget::event(event);
}

//--------------------------------------------------------
void NtetCircleWidget::mousePressEvent(QMouseEvent* event)
//--------------------------------------------------------
{
  if (handlePressAt(event->position()))
  {
    event->accept();
    return;
  }

  QWidget::mousePressEvent(event);
}

//------------------------------------------------------
bool NtetCircleWidget::handlePressAt(const QPointF& pos)
//------------------------------------------------------
{
  if (capturePresetRect_.contains(pos))
  {
    emit capturePresetRequested();
    return true;
  }

  if (edoLabelRect_.contains(pos))
  {
    emit edoLabelClicked();
    return true;
  }

  if (adaptingToggleRect_.contains(pos))
  {
    emit adaptingEnabledToggled();
    return true;
  }

  if (afterTouchRect_.contains(pos))
  {
    emit afterTouchBehaviourClicked();
    return true;
  }

  for (const auto& h : keyButtonHits_)
  {
    if (h.rect.contains(pos))
    {
      if (h.delta > 0)
        emit keyPitchRaiseRequested(h.keyIndex);
      else
        emit keyPitchLowerRequested(h.keyIndex);

      return true;
    }
  }

  for (const auto& h : hitAreas_)
  {
    if (h.rect.contains(pos))
    {
      emit tuningCenterSelected(h.value);
      return true;
    }
  }
  return false;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
void NtetCircleWidget::drawLeftStatusLabels(QPainter& p, const QPointF& center, double labelRadius, double outerRadius, const QFont& baseFont)
//--------------------------------------------------------------------------------------------------------------------------------------------
{
  QFont f = baseFont;
  f.setBold(true);
  p.setFont(f);

  QFontMetricsF fm(f);

  const double topY = center.y() - labelRadius;
  const double gap = -80.0;
  const double xRight = center.x() - outerRadius - gap;
  const double lineH = fm.height() * 1.45;

  const QString adaptingText = adaptingEnabled_
    ? QString::fromUtf8("✓ RT Adapting")
    : QString::fromUtf8("✕ RT Adapting");

  const QString afterText = afterTouchBehaviourText_;

  auto makeRect = [&](const QString& text, double y)
    {
      return QRectF(
        xRight - fm.horizontalAdvance(text) - 12.0,
        y - fm.height() / 2.0 - 4.0,
        fm.horizontalAdvance(text) + 12.0,
        fm.height() + 8.0);
    };

  adaptingToggleRect_ = makeRect(adaptingText, topY);
  afterTouchRect_ = makeRect(afterText, topY + lineH);

  p.setPen(adaptingEnabled_ ? kColorHighlightGreen : kColorDisabled);
  p.drawText(adaptingToggleRect_, Qt::AlignCenter, adaptingText);

  p.setPen(afterTouchEnabled_ ? kColorHighlightGreen : kColorDisabled);
  p.drawText(afterTouchRect_, Qt::AlignCenter, afterText);

  const QString tuningText = QStringLiteral("Tuning Center = %1").arg(noteNameFromFifths(config_->tuningCenter));

  QString keyModeText = isMinor_ ? "minor" : "major";

  const QString keyText =
    QStringLiteral("Key = %1 %2")
    .arg(noteNameFromFifths(keyTonic_))
    .arg(keyModeText);

  const QString chordRootText = chordName_.isEmpty() ? QStringLiteral("Chord Root = %1").arg(noteNameFromFifths(chordRoot_)) : "Chord = " + chordName_;

  const double bottomY = center.y() + labelRadius;

  QRectF tuningRect = makeRect(tuningText, bottomY - lineH);
  QRectF keyRect = makeRect(keyText, bottomY);
  QRectF chordRootRect = makeRect(chordRootText, bottomY + lineH);

  if (config_->tuningCenter != Config::invalid)
  {
    p.setFont(baseFont);
    p.setPen(kColorHighlightGold);
    p.drawText(tuningRect, Qt::AlignCenter, tuningText);
  }

  if (keyTonic_ != Config::invalid)
  {
    p.setPen(kColorHighlightCyan);
    p.drawText(keyRect, Qt::AlignCenter, keyText);
  }

  if (chordRoot_ != Config::invalid)
  {
    p.setPen(kColorHighlightRed);
    p.drawText(chordRootRect, Qt::AlignCenter, chordRootText);
  }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
void NtetCircleWidget::drawCapturePresetLabel(QPainter& p, const QPointF& center, const double labelRadius, const double outerRadius, const QFont& baseFont)
//----------------------------------------------------------------------------------------------------------------------------------------------------------
{
  const QString text = QString::fromUtf8("Keep it →");

  QFont f = baseFont;
  f.setBold(true);
  //f.setPointSizeF(baseFont.pointSizeF() * 0.9);

  QFontMetricsF fm(f);

  const double topY = center.y() - labelRadius;
  const double gap = -80.0;

  capturePresetRect_ = QRectF(
    center.x() + outerRadius + gap,
    topY - fm.height() / 2.0 - 4.0,
    fm.horizontalAdvance(text) + 12.0,
    fm.height() + 8.0
  );

  p.setFont(f);
  p.setPen(kColorHighlightDarkGold);
  p.drawText(capturePresetRect_, Qt::AlignCenter, text);
}

//---------------------------------------------------------------------
void NtetCircleWidget::drawKeyboardLabels(QPainter& p, const QRectF& r)
//---------------------------------------------------------------------
{
  if (!mapping_ || !config_)
    return;

  QFont font = p.font();
  font.setPointSizeF(r.height() * 0.085);
  font.setBold(true);
  p.setFont(font);

  const int values[12] =
  {
    config_->valueForKey[0],
    config_->valueForKey[1],
    config_->valueForKey[2],
    config_->valueForKey[3],
    config_->valueForKey[4],
    config_->valueForKey[5],
    config_->valueForKey[6],
    config_->valueForKey[7],
    config_->valueForKey[8],
    config_->valueForKey[9],
    config_->valueForKey[10],
    config_->valueForKey[11]
  };

  static constexpr double whiteX[7] =
  {
      0.075, 0.215, 0.355, 0.500, 0.645, 0.785, 0.925
  };

  static constexpr int whiteIndex[7] =
  {
      0, 2, 4, 5, 7, 9, 11
  };

  static constexpr double blackX[5] =
  {
      0.150, 0.290, 0.555, 0.725, 0.875
  };

  static constexpr int blackIndex[5] =
  {
      1, 3, 6, 8, 10
  };

  const int minValue = mapping_->minValue;

  for (int i = 0; i < 7; ++i)
  {
    const int key = whiteIndex[i];
    const int value = values[key];
    const ConfigMask bit = valueToPoolBit(value);
    const bool isPressed = (pressedMask5_ & bit) != 0;

    const QString name = noteNameFromFifths(value);

    QRectF textRect(
      r.left() + r.width() * whiteX[i] - r.width() * 0.055,
      r.top() + r.height() * 0.78,
      r.width() * 0.11,
      r.height() * 0.14
    );

    p.setPen(isPressed ? kColorHighlightDarkGreen : kColorBackground);
    p.drawText(textRect, Qt::AlignCenter, name);
  }

  QFont blackFont = p.font();
  blackFont.setPointSizeF(r.height() * 0.065);
  p.setFont(blackFont);

  for (int i = 0; i < 5; ++i)
  {
    const int key = blackIndex[i];
    const int value = values[key];
    const ConfigMask bit = valueToPoolBit(value);
    const bool isPressed = (pressedMask5_ & bit) != 0;

    const QString name = noteNameFromFifths(value);

    QRectF textRect(
      r.left() + r.width() * blackX[i] - r.width() * 0.045,
      r.top() + r.height() * 0.33,
      r.width() * 0.09,
      r.height() * 0.12
    );

    p.setPen(isPressed ? kColorHighlightGreen : Qt::white);
    p.drawText(textRect, Qt::AlignCenter, name);
  }
}

//--------------------------------------------------------------------------
void NtetCircleWidget::drawKeyboardStepButtons(QPainter& p, const QRectF& r)
//--------------------------------------------------------------------------
{
  static constexpr double xNorm[12] =
  {
      0.075, 0.145, 0.215, 0.285, 0.355, 0.500,
      0.575, 0.645, 0.715, 0.785, 0.855, 0.925
  };

  QFont f = p.font();
  f.setBold(true);
  f.setPointSizeF(r.height() * 0.13);
  p.setFont(f);

  QFontMetricsF fm(f);

  const double buttonH = fm.height() * 1.15;
  const double buttonW = r.width() * 0.055;

  for (int key = 0; key < 12; ++key)
  {
    const double x = r.left() + r.width() * xNorm[key];

    QRectF plusRect(
      x - buttonW / 2.0,
      r.top() - buttonH * 1.60,
      buttonW,
      buttonH * 1.60
    );

    const QRectF minusRect(
      x - buttonW / 2.0,
      r.bottom() + buttonH * 0.15,
      buttonW,
      buttonH
    );

    const bool plusEnabled = canRaiseKey_[key];
    const bool minusEnabled = canLowerKey_[key];

    p.setPen(plusEnabled ? kColorTextPrimary : kColorDisabledStepButtons);
    p.drawText(plusRect, Qt::AlignCenter, "+");

    p.setPen(minusEnabled ? kColorTextPrimary : kColorDisabledStepButtons);
    p.drawText(minusRect, Qt::AlignCenter, "-");

    if (plusEnabled)
      keyButtonHits_.push_back({ plusRect.adjusted(-6, -6, 6, 6), key, +1 });

    if (minusEnabled)
      keyButtonHits_.push_back({ minusRect.adjusted(-6, -6, 6, 6), key, -1 });
  }
}

//-------------------------------------------------------------------------------------------
int findBestSpellingForPitchStep(int pitchStep, int tuningCenter, const NtetMapping& mapping)
//-------------------------------------------------------------------------------------------
{
  int bestValue = 0;
  int bestDistance = INT_MAX;

  for (int value = kConfigMaskMin; value <= kConfigMaskMax; ++value)
  {
    if (mod(value * mapping.fifthStep, mapping.N) != pitchStep)
      continue;

    const int distance = std::abs(value - tuningCenter);

    if (distance < bestDistance)
    {
      bestDistance = distance;
      bestValue = value;
    }
  }

  return bestValue;
}

//------------------------------------------
void NtetCircleWidget::rebuildCircleLabels()
//------------------------------------------
{
  if (!mapping_ || !config_)
    return;

  labels_.clear();

  const int N = mapping_->N;
  const int referenceValue =
    config_->tuningCenter != Config::invalid
      ? config_->tuningCenter
      : 0;

  for (int pitchStep = 0; pitchStep < N; ++pitchStep)
  {
    int bestValue = Config::invalid;

    // Se questo grado è presente nella configurazione corrente,
    // conserva esattamente la grafia salvata nel preset.
    for (const int8_t value : config_->valueForKey)
    {
      if (mod(value * mapping_->fifthStep, mapping_->N) == pitchStep)
      {
        bestValue = value;
        break;
      }
    }

    // Per i gradi estranei ai dodici tasti scegli una grafia stabile.
    if (bestValue == Config::invalid)
    {
      bestValue = findBestSpellingForPitchStep(
        pitchStep,
        referenceValue,
        *mapping_);
    }

    CircleLabel label;
    label.value5 = bestValue;
    label.pitchStep = pitchStep;
    label.name = noteNameFromFifths(bestValue);

    labels_.push_back(label);
  }
}

//---------------------------------------------------
void NtetCircleWidget::paintEvent(QPaintEvent* event)
//---------------------------------------------------
{
  QWidget::paintEvent(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  p.fillRect(rect(), kColorBackground);

  hitAreas_.clear();
  keyButtonHits_.clear();

  if (!mapping_ || !config_)
    return;

  const QPointF center(width() / 2.0, height() / 2.0);

  const double size = qMin(width(), height());
  const double labelRadius = size * 0.40;

  QFont font = p.font();
  font.setPointSize(11);
  p.setFont(font);

  QFontMetricsF fm(font);

  const double ringHalfWidth = fm.height() * 0.9;
  const double outerRadius = labelRadius + ringHalfWidth;
  const double innerRadius = labelRadius - ringHalfWidth;
  const double centsRadius = innerRadius - 14.0;

  // Draw the two circles -------------------------------------------------

  p.setPen(QPen(kColorCircle, 1.5));
  p.drawEllipse(center, outerRadius, outerRadius);
  p.drawEllipse(center, innerRadius, innerRadius);

  //------------------------------------------------------------------------

  const int minValue = mapping_->minValue;
  const int maxValue = mapping_->maxValue;
  const int N = maxValue - minValue + 1;

  QFont baseFont = p.font();

  // Draw EDO label ----------------------------------------------------
  QString edoText = QString("%1 EDO ▾").arg(N);

  QFont edoFont = baseFont;
  edoFont.setBold(true);
  edoFont.setPointSizeF(baseFont.pointSizeF() * 1.2);

  p.setFont(edoFont);

  QFontMetricsF edoFm(edoFont);

  QPointF edoPos(
    center.x(),
    center.y() - centsRadius + edoFm.height() * 1.8
  );

  edoLabelRect_ = QRectF(
    edoPos.x() - edoFm.horizontalAdvance(edoText) / 2.0 - 6.0,
    edoPos.y() - edoFm.height() / 2.0,
    edoFm.horizontalAdvance(edoText) + 12.0,
    edoFm.height()
  );

  p.setPen(kColorHighlightDarkGold);
  p.drawText(edoLabelRect_, Qt::AlignCenter, edoText);

  //---------------------------------------------------------------------

  baseFont.setPointSize(11);

  for (const auto& label : labels_)
  {
    const double angle = -M_PI_2 + 2.0 * M_PI * double(label.pitchStep) / double(N);

    const double cents = 1200.0 * double(label.pitchStep) / double(N);
    QString centsText = QString::number(cents, 'f', 1);

    const QPointF pos(
      center.x() + labelRadius * std::cos(angle),
      center.y() + labelRadius * std::sin(angle)
    );

    const QRectF textRect(
      pos.x() - fm.horizontalAdvance(label.name) / 2.0 - 4.0,
      pos.y() - fm.height() / 2.0,
      fm.horizontalAdvance(label.name) + 8.0,
      fm.height()
    );

    const ConfigMask bit = valueToPoolBit(label.value5);

    const bool isSelected = (config_->mask & bit) != 0;
    const bool isPressed = (pressedMask5_ & bit) != 0;

    QColor color(kColorDisabled);

    if (isSelected)
      color = kColorHighlightDarkGold;

    if (isPressed)
      color = kColorHighlightGreen;

    QFont noteFont = baseFont;
    noteFont.setBold(isSelected || isPressed);

    if (isPressed)
      noteFont.setPointSizeF(baseFont.pointSizeF() + 2.0);
    else if (isSelected)
      noteFont.setPointSizeF(baseFont.pointSizeF() + 1.0);

    p.setFont(noteFont);
    p.setPen(color);
    p.drawText(textRect, Qt::AlignCenter, label.name);

    hitAreas_.push_back({ textRect, label.value5 });

    QFont centsFont = baseFont;
    centsFont.setPointSizeF(baseFont.pointSizeF() * 0.62);
    centsFont.setBold(false);

    QFontMetricsF centsFm(centsFont);

    QPointF centsPos(
      center.x() + centsRadius * std::cos(angle),
      center.y() + centsRadius * std::sin(angle)
    );

    QRectF centsRect(
      centsPos.x() - centsFm.horizontalAdvance(centsText) / 2.0,
      centsPos.y() - centsFm.height() / 2.0,
      centsFm.horizontalAdvance(centsText),
      centsFm.height()
    );

    p.setFont(centsFont);
    QColor centsColor(kColorCents);

    if (isSelected)
      centsColor = kColorHighlightGold;

    if (isPressed)
      centsColor = kColorHighlightGreen;

    p.setPen(centsColor);
    p.drawText(centsRect, Qt::AlignCenter, centsText);

    //Tuning center gold dot
    if (label.value5 == config_->tuningCenter)
    {
      const double dotRadiusFromCenter = outerRadius + 10.0;
      const double dotRadius = 4.5;

      QPointF dotPos(
        center.x() + dotRadiusFromCenter * std::cos(angle),
        center.y() + dotRadiusFromCenter * std::sin(angle)
      );

      p.setPen(Qt::NoPen);
      p.setBrush(kColorHighlightGold);
      p.drawEllipse(dotPos, dotRadius, dotRadius);
      p.setBrush(Qt::NoBrush);
    }

    //Key tonic cyan dot
    if (label.value5 == keyTonic_)
    {
      const double dotRadiusFromCenter = outerRadius + 16.0;
      const double dotRadius = 3.5;

      QPointF dotPos(
        center.x() + dotRadiusFromCenter * std::cos(angle),
        center.y() + dotRadiusFromCenter * std::sin(angle)
      );

      p.setPen(Qt::NoPen);
      p.setBrush(kColorHighlightCyan);
      p.drawEllipse(dotPos, dotRadius, dotRadius);
      p.setBrush(Qt::NoBrush);
    }

    //Chord root red dot
    if (label.value5 == chordRoot_)
    {
      const double dotRadiusFromCenter = outerRadius + 22.0;
      const double dotRadius = 2.8;

      QPointF dotPos(
        center.x() + dotRadiusFromCenter * std::cos(angle),
        center.y() + dotRadiusFromCenter * std::sin(angle)
      );

      p.setPen(Qt::NoPen);
      p.setBrush(kColorHighlightRed);
      p.drawEllipse(dotPos, dotRadius, dotRadius);
      p.setBrush(Qt::NoBrush);
    }
  }

  if (!keyboardPixmap_.isNull())
  {
    const double keyboardWidth = qMin(width() * 0.55, innerRadius * 1.25);
    const double keyboardHeight = keyboardWidth * double(keyboardPixmap_.height()) / double(keyboardPixmap_.width());

    QRectF keyboardRect(
      center.x() - keyboardWidth / 2.0,
      center.y() - keyboardHeight / 2.0,
      keyboardWidth,
      keyboardHeight
    );

    p.drawPixmap(
      keyboardRect.toRect(),
      keyboardPixmap_,
      keyboardPixmap_.rect()
    );

    drawKeyboardLabels(p, keyboardRect);
    drawKeyboardStepButtons(p, keyboardRect);
    drawLeftStatusLabels(p, center, labelRadius, outerRadius, baseFont);
    drawCapturePresetLabel(p, center, labelRadius, outerRadius, baseFont);
  }
}

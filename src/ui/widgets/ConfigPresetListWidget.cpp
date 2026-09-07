#include "ConfigPresetListWidget.h"
#include <QPainter>
#include <QEvent>
#include <QMouseEvent>
#include <QTouchEvent>

#include "../../StringUtilities.hpp"

namespace
{
  static const QColor kColorBackground{ 20, 20, 20 };

  static const QColor kColorItemBackground{ 45, 45, 45 };

  static const QColor kColorItemBorder{ 80, 80, 80 };
  static const QColor kColorItemBorderSelected{ 212, 175, 55 };

  static const QColor kColorTextPrimary{ 255, 255, 255 };

  static const QColor kColorDelete{ 170, 90, 90 };
}

//-------------------------------------------------------------------------------
ConfigPresetListWidget::ConfigPresetListWidget(QWidget* parent) : QWidget(parent)
//-------------------------------------------------------------------------------
{
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setAttribute(Qt::WA_AcceptTouchEvents, true);
}

//--------------------------------------------
QSize ConfigPresetListWidget::sizeHint() const
//--------------------------------------------
{
  QFont f = font();
  f.setPointSize(10);
  f.setBold(true);

  QFontMetrics fm(f);

  QString sample = "Dbb  C×  Dbb  Eb  E×  Fbb  F×  Gbb  Ab  Abb  Bb  Bbb    ×";

  const int w = fm.horizontalAdvance(sample) + 32;
  const int h = 300;

  return QSize(w, h);
}

//---------------------------------------------------
QSize ConfigPresetListWidget::minimumSizeHint() const
//---------------------------------------------------
{
  return QSize(320, 220);
}

//-----------------------------------------------------------------
void ConfigPresetListWidget::setMapping(const NtetMapping* mapping)
//-----------------------------------------------------------------
{
  mapping_ = mapping;
  update();
}

//-----------------------------------------------------------------------------------------------
void ConfigPresetListWidget::setPresets(const std::vector<Intona::Tuning::TuningPreset>& presets)
//-----------------------------------------------------------------------------------------------
{
  presets_ = &presets;
  update();
}

//-----------------------------------------------------------
void ConfigPresetListWidget::setCurrentPresetIndex(int index)
//-----------------------------------------------------------
{
  currentPresetIndex_ = index;
  update();
}

//---------------------------------------------------------
void ConfigPresetListWidget::paintEvent(QPaintEvent* event)
//---------------------------------------------------------
{
  QWidget::paintEvent(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), kColorBackground);

  hits_.clear();

  if (!mapping_ || !presets_)
    return;

  const double margin = 10.0;
  const double itemH = 42.0;
  const double gap = 8.0;

  QFont noteFont = p.font();
  noteFont.setPointSize(10);
  noteFont.setBold(true);

  for (int i = 0; i < int(presets_->size()); ++i)
  {
    QRectF itemRect(
      margin,
      margin + i * (itemH + gap),
      width() - 2.0 * margin,
      itemH
    );

    const bool current = (i == currentPresetIndex_);

    p.setPen(QPen(current ? kColorItemBorderSelected : kColorItemBorder, 1.5));
    p.setBrush(kColorItemBackground);
    p.drawRoundedRect(itemRect, 8, 8);

    QRectF deleteRect(
      itemRect.right() - 32.0,
      itemRect.top(),
      28.0,
      itemRect.height()
    );

    QRectF notesRect = itemRect.adjusted(8, 0, -36, 0);

    p.setFont(noteFont);
    p.setPen(kColorTextPrimary);

    const auto& cfg = (*presets_)[i];

    QStringList names;
    for (int key = 0; key < 12; ++key)
      names << noteNameFromFifths(cfg.values[key]);

    p.drawText(notesRect, Qt::AlignVCenter | Qt::AlignLeft, names.join("  "));

    p.setPen(kColorDelete);
    p.drawText(deleteRect, Qt::AlignCenter, QString::fromUtf8("×"));

    hits_.push_back({ itemRect, deleteRect, i });
  }
}

//------------------------------------------------------------
bool ConfigPresetListWidget::handlePressAt(const QPointF& pos)
//------------------------------------------------------------
{
  for (const auto& h : hits_)
  {
    if (h.deleteRect.contains(pos))
    {
      emit presetDeleteRequested(h.index, currentPresetIndex_);
      return true;
    }

    if (h.bodyRect.contains(pos))
    {
      emit presetSelected(h.index);
      return true;
    }
  }

  return false;
}

//--------------------------------------------------------------
void ConfigPresetListWidget::mousePressEvent(QMouseEvent* event)
//--------------------------------------------------------------
{
  if (handlePressAt(event->position()))
    return;

  QWidget::mousePressEvent(event);
}

//-----------------------------------------------
bool ConfigPresetListWidget::event(QEvent* event)
//-----------------------------------------------
{
  if (event->type() == QEvent::TouchBegin)
  {
    auto* touchEvent = static_cast<QTouchEvent*>(event);

    if (!touchEvent->points().isEmpty())
    {
      const QPointF pos = touchEvent->points().first().position();

      if (handlePressAt(pos))
      {
        event->accept();
        return true;
      }
    }
  }

  return QWidget::event(event);
}
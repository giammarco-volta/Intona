#include "SurfaceTab.h"

#include <QVBoxLayout>
#include <QLabel>
#include "../widgets/NtetCircleWidget.h"
#include "../widgets/ConfigPresetListWidget.h"


//-------------------------------------------------------
SurfaceTab::SurfaceTab(QWidget* parent) : QWidget(parent)
//-------------------------------------------------------
{
    QHBoxLayout* layout = new QHBoxLayout(this);

    ntetCircleWidget_ = new NtetCircleWidget(this);

    layout->addWidget(ntetCircleWidget_, 1);

    configPresetListWidget_ = new ConfigPresetListWidget(this);

    layout->addWidget(configPresetListWidget_, 0);
}

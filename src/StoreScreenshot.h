#pragma once
#include <QObject>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QSaveFile>
#include <QPainter>
#include <QQuickWindow>

// Render the current logical mobile layout directly into a full-resolution image.
class StoreScreenshot : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE void capture(QQuickItem* item, const QString& profile) {
        if (busy_) return;
        if (!item || (profile != "tablet" && profile != "phone") ||
            qAbs(item->width() / item->height() - (16.0 / 9.0)) > 0.001) {
            emit finished(false, tr("Select the phone or tablet simulation first."));
            return;
        }
        const QColor background = item->window() ? item->window()->color() : QColor("#202020");
        const QSize targetSize(1920, 1080);
        auto result = item->grabToImage(targetSize);
        if (!result) { emit finished(false, tr("Unable to capture this window.")); return; }
        busy_ = true;
        connect(result.data(), &QQuickItemGrabResult::ready, this, [this, result, profile, background, targetSize]() {
            const QImage captured = result->image();
            result->disconnect(this);
            const QString folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                + "/NaadaLab/Intona/GooglePlay/" + profile;
            const QString path = folder + "/Intona-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz") + ".png";
            // Dropping alpha directly unpremultiplies translucent panel colours.
            // Composite over the same opaque background used by the live window.
            QImage image(captured.size(), QImage::Format_RGB888);
            image.fill(background);
            {
                QPainter painter(&image);
                painter.drawImage(0, 0, captured);
            }
            QSaveFile file(path);
            const bool ok = image.size() == targetSize && QDir().mkpath(folder)
                && file.open(QIODevice::WriteOnly) && image.save(&file, "PNG")
                && file.size() <= 8 * 1024 * 1024 && file.commit();
            busy_ = false;
            emit finished(ok, ok ? path : tr("Unable to save a valid screenshot."));
        });
    }
signals:
    void finished(bool success, const QString& message);
private:
    bool busy_ = false;
};

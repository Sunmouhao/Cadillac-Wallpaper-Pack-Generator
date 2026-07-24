#include "CropPreviewWidget.h"

#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

CropPreviewWidget::CropPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(180);
    setAutoFillBackground(false);
    setMouseTracking(true);
}

void CropPreviewWidget::setImagePath(const QString &path)
{
    if (path == imagePath) {
        return;
    }
    imagePath = path;
    image = QImage();
    if (!path.trimmed().isEmpty() && QFileInfo::exists(path)) {
        image.load(path);
    }
    update();
}

void CropPreviewWidget::setTarget(const QString &newTitle, int width, int height)
{
    title = newTitle;
    targetWidth = qMax(1, width);
    targetHeight = qMax(1, height);
    update();
}

void CropPreviewWidget::setVerticalDividerPercent(qreal percent)
{
    dividerPercent = percent < 0 ? -1 : qBound<qreal>(0, percent, 100);
    update();
}

void CropPreviewWidget::setFocus(int horizontalPercent, int verticalPercent)
{
    focusX = qBound(0, horizontalPercent, 100);
    focusY = qBound(0, verticalPercent, 100);
    update();
}

void CropPreviewWidget::setZoom(int newZoomPercent)
{
    zoomPercent = qBound(100, newZoomPercent, 500);
    update();
}

QPoint CropPreviewWidget::focus() const
{
    return QPoint(focusX, focusY);
}

int CropPreviewWidget::zoom() const
{
    return zoomPercent;
}

void CropPreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(13, 18, 25));

    const QRectF frame = rect().adjusted(1, 1, -1, -1);
    painter.setPen(QPen(QColor(60, 74, 91), 1));
    painter.drawRoundedRect(frame, 4, 4);

    if (image.isNull()) {
        painter.setPen(QColor(150, 163, 184));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("选择 JPG/JPEG 后显示裁剪结果"));
        return;
    }

    const QRectF content = contentRect();
    const QRectF sourceCrop = cropRect(image.size(),
                                       targetWidth / static_cast<qreal>(targetHeight),
                                       focusX,
                                       focusY,
                                       zoomPercent);

    painter.save();
    painter.setClipRect(content);
    painter.drawImage(content, image, sourceCrop);
    painter.restore();

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(96, 165, 250), 2));
    painter.drawRect(content);

    if (dividerPercent >= 0) {
        QPen dividerPen(QColor(226, 232, 240), 2, Qt::DashLine);
        painter.setPen(dividerPen);
        const qreal x = content.left() + content.width() * dividerPercent / 100.0;
        painter.drawLine(QPointF(x, content.top()), QPointF(x, content.bottom()));
    }

    painter.fillRect(QRectF(content.left(), content.top(), content.width(), 28), QColor(0, 0, 0, 130));
    painter.setPen(QColor(226, 232, 240));
    painter.drawText(content.adjusted(8, 4, -8, -4), Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("%1  %2x%3").arg(title).arg(targetWidth).arg(targetHeight));
    painter.drawText(content.adjusted(8, 4, -8, -4), Qt::AlignRight | Qt::AlignTop,
                     QStringLiteral("X:%1%  Y:%2%  缩放:%3%").arg(focusX).arg(focusY).arg(zoomPercent));

    painter.fillRect(QRectF(content.left(), content.bottom() - 24, content.width(), 24), QColor(0, 0, 0, 120));
    painter.setPen(QColor(203, 213, 225));
    painter.drawText(content.adjusted(8, 0, -8, -4), Qt::AlignLeft | Qt::AlignBottom,
                     QStringLiteral("拖动调整位置，滚轮缩放，多余部分不显示"));
}

void CropPreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (image.isNull() || !contentRect().contains(event->pos())) {
        return;
    }
    dragging = true;
    dragStartPoint = event->pos();
    dragStartFocus = QPoint(focusX, focusY);
}

void CropPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging) {
        return;
    }
    updateFocusFromDrag(event->pos());
}

void CropPreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (dragging) {
        updateFocusFromDrag(event->pos());
    }
    dragging = false;
}

void CropPreviewWidget::wheelEvent(QWheelEvent *event)
{
    const QPoint wheelPosition = event->position().toPoint();
    if (image.isNull() || !contentRect().contains(wheelPosition)) {
        event->ignore();
        return;
    }

    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) {
        event->accept();
        return;
    }
    const int oldZoom = zoomPercent;
    setZoom(zoomPercent + steps * 10);
    if (oldZoom != zoomPercent) {
        emit focusChanged(focusX, focusY);
    }
    event->accept();
}

QRectF CropPreviewWidget::cropRect(const QSizeF &sourceSize, qreal targetAspect,
                                   int horizontalPercent, int verticalPercent, int zoomPercent)
{
    if (sourceSize.isEmpty() || targetAspect <= 0) {
        return QRectF();
    }

    const qreal sourceAspect = sourceSize.width() / sourceSize.height();
    qreal cropWidth = sourceSize.width();
    qreal cropHeight = sourceSize.height();

    if (sourceAspect > targetAspect) {
        cropWidth = sourceSize.height() * targetAspect;
    } else {
        cropHeight = sourceSize.width() / targetAspect;
    }

    const qreal zoom = qBound(100, zoomPercent, 500) / 100.0;
    cropWidth = qMax<qreal>(1.0, cropWidth / zoom);
    cropHeight = qMax<qreal>(1.0, cropHeight / zoom);

    const qreal maxX = qMax<qreal>(0, sourceSize.width() - cropWidth);
    const qreal maxY = qMax<qreal>(0, sourceSize.height() - cropHeight);
    const qreal x = maxX * horizontalPercent / 100.0;
    const qreal y = maxY * verticalPercent / 100.0;
    return QRectF(x, y, cropWidth, cropHeight);
}

QPoint CropPreviewWidget::focusFromCropTopLeft(const QSizeF &sourceSize, qreal targetAspect,
                                               const QPointF &sourceTopLeft, int zoomPercent)
{
    const QRectF centered = cropRect(sourceSize, targetAspect, 50, 50, zoomPercent);
    const qreal maxX = qMax<qreal>(0, sourceSize.width() - centered.width());
    const qreal maxY = qMax<qreal>(0, sourceSize.height() - centered.height());
    const qreal left = qBound<qreal>(0, sourceTopLeft.x(), maxX);
    const qreal top = qBound<qreal>(0, sourceTopLeft.y(), maxY);
    const int x = maxX <= 0 ? 50 : qRound(left * 100.0 / maxX);
    const int y = maxY <= 0 ? 50 : qRound(top * 100.0 / maxY);
    return QPoint(qBound(0, x, 100), qBound(0, y, 100));
}

QRectF CropPreviewWidget::contentRect() const
{
    const QRectF frame = rect().adjusted(10, 10, -10, -10);
    QSizeF targetSize(targetWidth, targetHeight);
    targetSize.scale(frame.size(), Qt::KeepAspectRatio);
    return QRectF(frame.center().x() - targetSize.width() / 2.0,
                  frame.center().y() - targetSize.height() / 2.0,
                  targetSize.width(),
                  targetSize.height());
}

QPointF CropPreviewWidget::cropTopLeftForFocus(int horizontalPercent, int verticalPercent) const
{
    if (image.isNull()) {
        return QPointF();
    }
    const QRectF current = cropRect(image.size(),
                                    targetWidth / static_cast<qreal>(targetHeight),
                                    horizontalPercent,
                                    verticalPercent,
                                    zoomPercent);
    return current.topLeft();
}

void CropPreviewWidget::updateFocusFromPoint(const QPointF &widgetPoint)
{
    Q_UNUSED(widgetPoint);
}

void CropPreviewWidget::updateFocusFromDrag(const QPointF &widgetPoint)
{
    if (image.isNull()) {
        return;
    }

    const QRectF content = contentRect();
    if (content.isEmpty()) {
        return;
    }

    const QRectF startCrop = cropRect(image.size(),
                                      targetWidth / static_cast<qreal>(targetHeight),
                                      dragStartFocus.x(),
                                      dragStartFocus.y(),
                                      zoomPercent);
    const QPointF delta = widgetPoint - dragStartPoint;
    const qreal sourceDx = delta.x() * startCrop.width() / content.width();
    const qreal sourceDy = delta.y() * startCrop.height() / content.height();
    const QPoint focus = focusFromCropTopLeft(
                image.size(),
                targetWidth / static_cast<qreal>(targetHeight),
                QPointF(startCrop.left() - sourceDx, startCrop.top() - sourceDy),
                zoomPercent);

    if (focus.x() == focusX && focus.y() == focusY) {
        return;
    }
    focusX = focus.x();
    focusY = focus.y();
    emit focusChanged(focusX, focusY);
    update();
}

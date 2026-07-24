#pragma once

#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QWidget>

class QMouseEvent;
class QWheelEvent;

class CropPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CropPreviewWidget(QWidget *parent = nullptr);

    void setImagePath(const QString &path);
    void setTarget(const QString &title, int width, int height);
    void setVerticalDividerPercent(qreal percent);
    void setFocus(int horizontalPercent, int verticalPercent);
    void setZoom(int zoomPercent);
    QPoint focus() const;
    int zoom() const;

signals:
    void focusChanged(int horizontalPercent, int verticalPercent);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    static QRectF cropRect(const QSizeF &sourceSize, qreal targetAspect,
                           int horizontalPercent, int verticalPercent, int zoomPercent);
    static QPoint focusFromCropTopLeft(const QSizeF &sourceSize, qreal targetAspect,
                                       const QPointF &sourceTopLeft, int zoomPercent);
    QRectF contentRect() const;
    QPointF cropTopLeftForFocus(int horizontalPercent, int verticalPercent) const;
    void updateFocusFromPoint(const QPointF &widgetPoint);
    void updateFocusFromDrag(const QPointF &widgetPoint);

    QImage image;
    QString imagePath;
    QString title;
    int targetWidth = 3950;
    int targetHeight = 1320;
    qreal dividerPercent = -1;
    int focusX = 50;
    int focusY = 50;
    int zoomPercent = 100;
    bool dragging = false;
    QPointF dragStartPoint;
    QPoint dragStartFocus;
};

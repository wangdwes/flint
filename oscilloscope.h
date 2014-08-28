#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include <QWidget>
#include <QBitmap>
#include <QPaintEvent>

class Oscilloscope : public QWidget
{
    Q_OBJECT
public:

    // A position-marker, as its name suggests, marks a position. It can either be a horizontal
    // or vertical cursor typically found on an oscilloscope, or a channel marker that tells
    // where the baseline of the data for a channel should reside at.

    // The marker is graphically represented by an arrow that is drawn at all times. If the
    // currently marked posi tion is within the visible viewport, the arrow will be placed at
    // the corresponding position with proper orientation; otherwise, the arrow will be docked
    // at the borders of the plot area and orientated to indicate where the marked position is.

    struct PositionMarker {
    public:

        int position;
        int dockDepth;
        int dockOffset;
        bool isActive;
        bool isVertical;
        QRect lineRect;
        QRect pointerRect;

        QBitmap originalPointerBitmap;
        QBitmap orientatedPointerBitmap;

    };








    explicit Oscilloscope(QWidget *parent = 0);

    void moveViewport(const QPoint& delta);

protected:
    bool event(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    void buildPaintCache();
    void updateCursorGeometry();

    QVector<QPoint> verticalMajorDots;
    QVector<QPoint> horizontalMajorDots;
    QVector<QPoint> verticalMinorDots;
    QVector<QPoint> horizontalMinorDots;
    QVector<QLine>  verticalTicks;
    QVector<QLine>  horizontalTicks;

    const static int plotAreaMarginLeft = 31;
    const static int plotAreaMarginTop = 21;
    const static int plotAreaMarginRight = 21;
    const static int plotAreaMarginBottom = 21;

    const static int cursorPointerWidthAnchor = 4;
    const static int cursorPointerHeightAnchor = 11;
    const static int cursorDockOffsetBase = 3;
    const static int cursorDockOffsetIncrement = 10;
    const static int cursorDefaultPositionBase = 200;
    const static int cursorDefaultPositionIncrement = 20;

    const static int verticalRulerDistance = 120;
    const static int horizontalRulerDistance = 150;
    const static int verticalMajorDistance = 30;
    const static int horizontalMajorDistance = 30;
    const static int verticalMinorDistance = 6;
    const static int horizontalMinorDistance = 6;
    const static int tickMarkLength = 2;

    QRect borderRect;
    QRect horizontalScrollRect;
    QRect verticalScrollRect;

    QRect plotAreaRect;
    QRect currentViewport;
    QRect maximumViewport;

signals:

public slots:

};

#endif // OSCILLOSCOPE_H

#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include <QDebug>
#include <QWidget>
#include <QBitmap>
#include <QPaintEvent>

class Oscilloscope : public QWidget
{
    Q_OBJECT

    // A marker marks a position on the oscilloscope view. It can either be a horizontal or
    // vertical cursor typically found on a physical oscilloscope, or a pointer that indicates
    // zero baseline of a particular data channel.

    // The marker is graphically represented by an arrow that is always drawn. If the currently
    // marked position is within the visible viewport, the arrow will be placed at that position
    // and facing the center of the plot area with designated orientation; otherwise, the arrow
    // will be docked at the appropriate edges, facing outside.

    struct Marker {

        enum Edge { Top = 0, Right = 1, Bottom = 2, Left = 3 };

        int depth;
        int ceiling;    // distance between the top-most pixel of the bitmap to the edge.
        int position;   // the currently marked position, within the viewport.
        int deadzone;   // the no-go zone next to the docks.
        bool isActive;
        Edge mountEdge;         // on which edge is this marker mounted?
        QRect drawRect;         // the rect in which the drawBitmap is drawn.
        QLine drawLine;         // a line that visually hints the position.
        QPoint dockPosition;    // the position at which the anchor should be at when docked.
        QRect sensitiveRect;    // the mouse-senstitive rect.
        QBitmap drawBitmap;
        QBitmap undockedBitmap;       // when the marker is undocked,
        QBitmap lowerDockedBitmap;    // when docked at the lower dock,
        QBitmap upperDockedBitmap;    // when docked at the upper dock.

        // The complier does not automatically generate a method to instantiate a structure like this,
        // therefore we would have to write a small simple static function.

        static Marker instantiate(int ceiling, int position, int deadzone, Edge mountEdge,
            const QPoint& dockPosition, const QBitmap& dockedBitmap, const QBitmap& undockedBitmap) {

            static QTransform clockwiseRotation(0, 1, -1, 0, 0, 0);
            static QTransform anticlockwiseRotation(0, -1, 1, 0, 0, 0);

            Marker instance;
            instance.depth = 0;
            instance.ceiling = ceiling;
            instance.position = position;
            instance.deadzone = deadzone;
            instance.isActive = true;
            instance.mountEdge = mountEdge;
            instance.dockPosition = dockPosition;
            instance.undockedBitmap = undockedBitmap;
            instance.lowerDockedBitmap = dockedBitmap.transformed(clockwiseRotation);
            instance.upperDockedBitmap = dockedBitmap.transformed(anticlockwiseRotation);

            for (int index = 0; index < instance.mountEdge; index++)
                instance.undockedBitmap = instance.undockedBitmap.transformed(clockwiseRotation);

            for (int index = 0; index < instance.mountEdge % 2; index++) {
                instance.lowerDockedBitmap = instance.lowerDockedBitmap.transformed(clockwiseRotation);
                instance.upperDockedBitmap = instance.upperDockedBitmap.transformed(clockwiseRotation); }

            return instance;

        }   // this data structure is shared among
    };      // instances of cursors and channel baseline indicators.

public:
    explicit Oscilloscope(QWidget *parent = 0);
    void moveViewport(const QPoint& delta);

protected:
    bool event(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    void buildPaintCache();
    int updateMarkerGeometry(Marker *marker);

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

    const static int tickMarkLength = 2;
    const static int verticalRulerDistance = 120;
    const static int horizontalRulerDistance = 150;
    const static int verticalMajorDistance = 30;
    const static int horizontalMajorDistance = 30;
    const static int verticalMinorDistance = 6;
    const static int horizontalMinorDistance = 6;

    const static int cursorCeiling = 1;
    const static int cursordeadzone = 0;
    const static int cursorDockOffsetBase = 3;
    const static int cursorDockOffsetIncrement = 10;
    const static int cursorDefaultPositionBase = 200;
    const static int cursorDefaultPositionIncrement = 20;

    QRect borderRect;
    QRect horizontalScrollRect;
    QRect verticalScrollRect;

    QRect plotAreaRect;
    QRect currentViewport;
    QRect maximumViewport;

    QList<Marker*> cursors;
    Marker testMarker, testMarker2;
    Marker testMarker3, testMarker4;

signals:

public slots:

};

#endif // OSCILLOSCOPE_H

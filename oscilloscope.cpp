#include "oscilloscope.h"

#include <QDebug>
#include <QPainter>
#include <QPaintEvent>

Oscilloscope::Oscilloscope(QWidget *parent) :
    QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    setAttribute(Qt::WA_AcceptDrops);
    setAttribute(Qt::WA_ForceUpdatesDisabled);

    // Indicates that the widget has no background, i.e, when the widget
    // receives paint events, the background is not automatically repainted.
    setAttribute(Qt::WA_NoSystemBackground, false);

    // Indicates that the widget contents are north-west aligned and static. On resize,
    // such a widget will receive paint events only for parts of itself that are newly visible.
    setAttribute(Qt::WA_StaticContents, true);

    // Indicates that the widget paints all its pixels when it receives paint event.
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    maximumViewport.setRect(0, 0, 1200, 800);
    currentViewport.setTopLeft(QPoint(0, 0));

    verticalMajorDots.append(QPoint(0, 0));
    horizontalMajorDots.append(QPoint(0, 0));
    verticalMinorDots.append(QPoint(0, 0));
    horizontalMinorDots.append(QPoint(0, 0));
    verticalTicks.append(QLine(0, 0, tickMarkLength, 0));
    horizontalTicks.append(QLine(0, 0, 0, tickMarkLength));
}

void Oscilloscope::moveViewport(const QPoint &delta)
{

    // Moves the viewport by delta. This method accepts the movement only when the new viewport
    // still resides within the viewport extremes. As the viewport is being moved, we scroll the
    // widget horizontally and vertically, which in turn leads to repainting of the newly exposed area.

    QRect proposedViewport = currentViewport.translated(delta);
    if (!maximumViewport.contains(proposedViewport)) return;

    currentViewport = proposedViewport;
    scroll (0 - delta.x(), 0, horizontalScrollRect);
    scroll (0, 0 - delta.y(), verticalScrollRect);

    // Some flickers can still be observed when scrolling, c.f.:
    // http://stackoverflow.com/questions/10173348/how-to-avoid-perceived-flicker-during-scrolling-in-qt

}

void Oscilloscope::updateCursorGeometry(PositionMarker *cursor)
{
    int lowerBound = cursor->isVertical ? currentViewport.top(): currentViewport.left();
    int uppreBound = cursor->isVertical ? currentViewport.bottom(): currentViewport.right();

}

bool Oscilloscope::event(QEvent *event)
{

    // Note that at this point keyEvent can be null, as we cannot guarantee that
    // event is a key-event. We do this such that we do not have to do multiple casts
    // later each time event is referenced.

    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

    switch (event->type()) {
    case QEvent::KeyPress:
        if (keyEvent->key() == Qt::Key_W) moveViewport(QPoint(0, 1));
        if (keyEvent->key() == Qt::Key_S) moveViewport(QPoint(0 ,-1));
        if (keyEvent->key() == Qt::Key_A) moveViewport(QPoint(1, 0));
        if (keyEvent->key() == Qt::Key_D) moveViewport(QPoint(-1, 0));
    default: ; }

    return QWidget::event(event);
}

void Oscilloscope::resizeEvent(QResizeEvent *event)
{

    // The plot-area is where the gridlines and the waveforms are plotted. It typically has
    // a pre-defined margin to the entire available space, allowing auxiliary elements such
    // as cursor arrows and markers to dock in between. Note that the tick marks are plotted
    // outside of the plot-area.

    plotAreaRect = rect().adjusted(
        0 + plotAreaMarginLeft, 0 + plotAreaMarginTop,
        0 - plotAreaMarginRight, 0 - plotAreaMarginBottom);

    currentViewport.setSize(plotAreaRect.size());

    // The following are rects in which scrolling is performed when the user moves the viewport.

    horizontalScrollRect = plotAreaRect.adjusted(0, 0 - tickMarkLength, 0, 0 + tickMarkLength);
    verticalScrollRect = plotAreaRect.adjusted(0 - tickMarkLength, 0, 0 + tickMarkLength, 0);
    borderRect = horizontalScrollRect.united(verticalScrollRect);

    // === BEGINNING OF GRID-LINE CACHE BUILD ===

    static QPoint horizontalMajorStep(horizontalMajorDistance, 0);
    static QPoint verticalMajorStep(0, verticalMajorDistance);
    static QPoint horizontalMinorStep(horizontalMinorDistance, 0);
    static QPoint verticalMinorStep(0, verticalMinorDistance);

    // The size of the actual plotting area is usually smaller than that of the widget.
    // So here we define a new variableto make maintainence easier. Also we use a macro to
    // trick the parser such that the variable size does not show as unused and produce a warning.

    QSize size = event->size(); Q_UNUSED(size);

    // A convenience macro that helps building the cache. If the cache contains more
    // points than required, the tailing points are removed, otherwise more points are appended.

    // CONT = CONTAINER in which the cache is stored.
    // AXIS = AXIS on which the points are distributed.
    // EXT = EXTENT that points should reach on axis AXIS.
    // STEP = STEP that defines the distance between two neighboring points in the cache.

#define BUILD_DOTCACHE(CONT, AXIS, EXT, STEP) { \
        while (CONT.last().AXIS() > size.EXT()) CONT.removeLast(); \
        while (CONT.last().AXIS() < size.EXT()) CONT.append(CONT.last() + STEP); }

    BUILD_DOTCACHE (verticalMajorDots, y, height, verticalMajorStep);
    BUILD_DOTCACHE (horizontalMajorDots, x, width, horizontalMajorStep);
    BUILD_DOTCACHE (verticalMinorDots, y, height, verticalMinorStep);
    BUILD_DOTCACHE (horizontalMinorDots, x, width, horizontalMinorStep);

#undef BUILD_DOTCACHE

    // Yet another convenience macro that helps building the cache. This macro performs exactly
    // the same functionality with the one above, but differs slightly due to different method names.

    // CONT = CONTAINER in which the cache is stored.
    // COMP = COMPONENT of a line that is involved in the comparsion with extent EXT.
    // EXT = EXTENT that the component COMP of the lines in the cache should reach.
    // STEP = STEP that defines the distance between two     neighboring lines in the cache.

#define BUILD_TICKCACHE(CONT, COMP, EXT, STEP) { \
        while (CONT.last().COMP() > size.EXT()) CONT.removeLast(); \
        while (CONT.last().COMP() < size.EXT()) CONT.append(CONT.last().translated(STEP)); }

    BUILD_TICKCACHE (verticalTicks, y1, height, verticalMinorStep);
    BUILD_TICKCACHE (horizontalTicks, x1, width, horizontalMinorStep);

#undef BUILD_TICKCACHE

}



void Oscilloscope::paintEvent(QPaintEvent *event)
{
    QPainter painter(this); QRect viewportRect;
    QPoint viewportToPlotArea = currentViewport.topLeft() - plotAreaRect.topLeft();

    // Two convenience macros that return the minimum multiple of modulus MOD that is greater/ no less
    // than the operand OP, operations used extensively when computing offsets.

#define CEIL(OP, MOD) ((OP) - (OP) % (MOD) + ((OP) % (MOD) ? (MOD): 0))
#define FLOOR(OP, MOD) ((OP) - (OP) % (MOD))

    // The following static variables actually appear in multiple methods,
    // but we don't care about memory usage so it doesn't really matter.

    static QPoint horizontalMajorStep(horizontalMajorDistance, 0);
    static QPoint verticalMajorStep(0, verticalMajorDistance);
    static QPoint horizontalMinorStep(horizontalMinorDistance, 0);
    static QPoint verticalMinorStep(0, verticalMinorDistance);
    static QPoint horizontalRulerStep(horizontalRulerDistance, 0);
    static QPoint verticalRulerStep(0, verticalRulerDistance);

    // A convenience macro that plots elements repetitively. A plausibly more encouraged approach
    // is to write it as an inline method, but there are just too many parameters to pass.

    // OFFSET = OFFSET at which the plotting begins.
    // AXIS = AXIS on which the element in the cache are plotted iteratively.
    // ALTAXIS = ALTERNATE AXIS on which the elements in the cache reside on.
    // STEP = STEP by which the elements are plotted on axis AXIS.
    // CACHE = CACHE that contains the contents to be plotted.
    // DIS = DISTANCE of any two neighboring elements in the cache.

    // Note that OFFSET must be stored in a variable,
    // otherwise value-assignments on it have no effect.

#define DRAW(OFFSET, AXIS, ALTAXIS, STEP, CACHE, DIS) { \
    QPoint offset = OFFSET; \
    int count = ((viewportRect.bottomRight() - offset)).ALTAXIS() / DIS + 1; \
    if (count > 0) { painter.save(); painter.translate(offset); \
        while ((viewportRect.bottomRight() - offset).AXIS() >= 0) { \
            painter.DRAWER(CACHE.data(), count); \
            painter.translate(STEP); offset += STEP; \
        }   painter.restore(); } }

    painter.save();
    painter.setPen(Qt::white);
    painter.drawRect(borderRect.adjusted(-1, -1, 0, 0));
    painter.setClipRect(borderRect);
    painter.translate(-viewportToPlotArea);

    foreach (QRect rect, event->region().rects()) {

        painter.fillRect(rect.translated(viewportToPlotArea), Qt::black);
        viewportRect = rect.intersected(plotAreaRect).translated(viewportToPlotArea);

        int verticalMinorOffset = CEIL(viewportRect.top(), verticalMinorDistance);
        int horizontalMinorOffset = CEIL(viewportRect.left(), horizontalMinorDistance);
        int verticalMajorOffset = CEIL(viewportRect.top(), verticalMajorDistance);
        int horizontalMajorOffset = CEIL(viewportRect.left(), horizontalMajorDistance);
        int verticalRulerOffset = FLOOR(viewportRect.top(), verticalRulerDistance) - (tickMarkLength >> 1);
        int horizontalRulerOffset = FLOOR(viewportRect.left(), horizontalRulerDistance) - (tickMarkLength >> 1);

        // The following code plots the minor and major grids (dots or linees) if set visible.
        // To minimize the number of invocations of the drawing method, we use different plotting
        // strategies based on the geometric shape of the plotting area.

        // 1)   If the horizontal extent is greater than the vertical extent, plot horizontal
        //      minors aligned with vertical majors, and horizontal majors with vertical minors.

        // 2)   If the vertical extent is greater than the horizontal extent, plot vertical
        //      minors aligned with horizontal majors, and vertical majors with horizontal minors.

#define DRAWER drawPoints

        if (viewportRect.width() > viewportRect.height()) {

            DRAW (QPoint(horizontalMinorOffset, verticalMajorOffset), y, x,
                  verticalMajorStep, horizontalMinorDots, horizontalMinorDistance);
            DRAW (QPoint(horizontalMajorOffset, verticalMinorOffset), y, x,
                  verticalMinorStep, horizontalMajorDots, horizontalMajorDistance); }

        else {

            DRAW (QPoint(horizontalMinorOffset, verticalMajorOffset), x, y,
                  horizontalMinorStep, verticalMajorDots, verticalMajorDistance);
            DRAW (QPoint(horizontalMajorOffset, verticalMinorOffset), x, y,
                  horizontalMajorStep, verticalMinorDots, verticalMinorDistance); }

#undef DRAWER

#define DRAWER drawLines

        // The following code plots the rulers that serve as a even higher-level gridlines
        // than majors, in the form of tick marks. Technically we can as well use different
        // strategies when plotting, but the increased performance is trivial in comparison
        // to the lost readability of the code.

        DRAW (QPoint(horizontalRulerOffset, verticalMinorOffset), x, y,
              horizontalRulerStep, verticalTicks, verticalMinorDistance);
        DRAW (QPoint(horizontalMinorOffset, verticalRulerOffset), y, x,
              verticalRulerStep, horizontalTicks, horizontalMinorDistance);

        // The following code plots the tick marks that reside on the axe. These elements
        // differ different all others because one of their dimensions never changes,
        // as the viewport is being moved around.

        viewportRect = rect.intersected(borderRect).translated(viewportToPlotArea);

        DRAW (QPoint(horizontalMinorOffset, currentViewport.top() - 1 - tickMarkLength), y, x,
              borderRect.bottomRight(), horizontalTicks, horizontalMinorDistance);
        DRAW (QPoint(horizontalMinorOffset, currentViewport.bottom() + 1), y, x,
              borderRect.bottomRight(), horizontalTicks, horizontalMinorDistance);
        DRAW (QPoint(currentViewport.left() - 1 - tickMarkLength, verticalMinorOffset), x, y,
              borderRect.bottomRight(), verticalTicks, verticalMinorDistance);
        DRAW (QPoint(currentViewport.right() + 1, verticalMinorOffset), x, y,
              borderRect.bottomRight(), verticalTicks, verticalMinorDistance);

#undef DRAWER

    }

    painter.restore();

}


















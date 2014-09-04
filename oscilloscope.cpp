#include "oscilloscope.h"

#include <QTime>
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>

Oscilloscope::Oscilloscope(QWidget *parent) :
    QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    setAttribute(Qt::WA_AcceptDrops);
    setAttribute(Qt::WA_ForceUpdatesDisabled);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAttribute(Qt::WA_StaticContents, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    maximumViewport.setRect(0, 0, 1200, 800);
    currentViewport.setTopLeft(QPoint(400, 0));

    verticalMajorDots.append(QPoint(0, 0));
    horizontalMajorDots.append(QPoint(0, 0));
    verticalMinorDots.append(QPoint(0, 0));
    horizontalMinorDots.append(QPoint(0, 0));
    verticalTicks.append(QLine(0, 0, tickMarkLength, 0));
    horizontalTicks.append(QLine(0, 0, 0, tickMarkLength));

    testMarker = Marker::instantiate(1, 400, 1, Marker::Bottom, QPoint(1, 1),
                                     QBitmap("://images/hcursor-1.bmp"),
                                     QBitmap("://images/hcursor-1.bmp"));

    testMarker2 = Marker::instantiate(1, 350, 1, Marker::Bottom, QPoint(1, 11),
                                     QBitmap("://images/hcursor-2.bmp"),
                                     QBitmap("://images/hcursor-2.bmp"));

    testMarker3 = Marker::instantiate(1, 400, 1, Marker::Left, QPoint(1, 1),
                                     QBitmap("://images/vcursor-1.bmp"),
                                     QBitmap("://images/vcursor-1.bmp"));

    testMarker4 = Marker::instantiate(1, 350, 1, Marker::Left, QPoint(1, 11),
                                     QBitmap("://images/vcursor-2.bmp"),
                                     QBitmap("://images/vcursor-2.bmp"));


    cursors.append(&testMarker);
    cursors.append(&testMarker2);
    cursors.append(&testMarker3);
    cursors.append(&testMarker4);
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

#define IS_HORIZONTAL(M) (((M)->mountEdge == Marker::Top)  || ((M)->mountEdge == Marker::Bottom))
#define IS_VERTICAL(M)   (((M)->mountEdge == Marker::Left) || ((M)->mountEdge == Marker::Right))

    for (int index = 0; index < cursors.count(); index++) {
        Marker *marker = cursors.value(index); int oldDepth = marker->depth;
        QRect oldDrawRect = marker->drawRect, oldSensitiveRect = marker->sensitiveRect;

        // There are three rects involved here: the OLD and the NEW rects are rects before
        // and after the update respectively, and the translated rect is the old rect
        // translated by amount negative delta.

        // If the depth has changed, either its docking status has changed, meaning that
        // no rect completely overlaps with one another (since docked and undocked markers
        // typically have different geometries), and we would have to update all three rects;
        // or, the marker is docked and remains to be docked, meaning that the old rect
        // and the new rect completely overlap each other, but it still doesn't hurt to
        // update that region twice, thanks to the underlying framework.

        if (marker->depth != updateMarkerGeometry(marker)) {
            update (oldDrawRect);
            update (oldDrawRect.translated(-delta));
            update (marker->drawRect); }

        // If the depth remains unchanged, the marker is undocked and continues to be undocked.
        // If the viewport is only scrolled with respect to the axis on which the marker is mounted,
        // the marker has itself scrolled by appropriate amount with the grid-lines, eliminating
        // the need to redraw anything. However, if the scroll vector has non-zero component
        // on the perpendicular axis, the marker would have been scrolled to the wrong place,
        // and we would have to clean the 'wrong place' and redraw it at where it should be.

        else {
            if (delta.x() == 0 && IS_VERTICAL(marker)) continue;
            if (delta.y() == 0 && IS_HORIZONTAL(marker)) continue;
            update (oldDrawRect);
            update (oldDrawRect.translated(-delta)); }

        // Note that the region class does not accept rects with zero height or width,
        // therefore if we want to draw or clean the draw-line, we would have to update
        // a larger area, and the sensitive-area seems to be a good candidate.

        if (marker->depth == 0 && oldDepth != 0) update (marker->sensitiveRect); // draw.
        if (marker->depth != 0 && oldDepth == 0) update (oldSensitiveRect); // clean.

    }

#undef IS_HORIZONTAL
#undef IS_VERTICAL

}



void Oscilloscope::moveMarker(Marker *marker, const QPoint& delta)
{

    if (1) {

        int proposedPosition = marker->position + delta.x();
        if (proposedPosition <= maximumViewport.right() && proposedPosition >= maximumViewport.left()) {
            marker->position = proposedPosition; else return; }

    else if (2) {

        int proposedPosition = marker->position + delta.y();
        if (proposedPosition <= maximumViewport.bottom() && proposedPosition >= maximumViewport.top())
            marker->position = proposedPosition; else return; }



#define IS_HORIZONTAL(M) (((M)->mountEdge == Marker::Top)  || ((M)->mountEdge == Marker::Bottom))
#define IS_VERTICAL(M)   (((M)->mountEdge == Marker::Left) || ((M)->mountEdge == Marker::Right))



#undef IS_HORIZONTAL
#undef IS_VERTICAL

}



int Oscilloscope::updateMarkerGeometry(Marker *marker)
{

    int halfWidth = marker->undockedBitmap.width() >> 1;
    int halfHeight = marker->undockedBitmap.height() >> 1, proposedDepth = 0;

    // I tried to share as much code as possible between the horizontal case and the vertical
    // case. It has turned out that the new code is shorter indeed, but it has also become
    // less intuitive and straightforward. I decided to stick with the original approach.

    // In both cases, the colliding volume of the marker is compared with the bounds
    // to determine whether the marker should be docked, and if so, where. To avoid
    // slow operation such as bitmap transformation, we have prepared markers of all
    // possible orientations on object instantiation. Here we simply assign one of them
    // to our draw-bitmap, thanks to the implicit sharing mechanism.

    switch (marker->mountEdge) {
    case Marker::Top:
    case Marker::Bottom:

        // This is the horizontal case, i.e., the marker marks a position on the horizontal axis.
        // The layout is always done as if it were mounted on the top edge, and additional
        // processing is carried out afterwards.

        if ((proposedDepth = marker->position - halfWidth - currentViewport.left() - marker->deadzone) < 0) {
            marker->depth = proposedDepth;
            marker->drawBitmap = marker->lowerDockedBitmap;
            marker->drawRect.setSize(marker->drawBitmap.size());
            marker->drawRect.moveTop(marker->dockPosition.y());
            marker->drawRect.moveLeft(marker->dockPosition.x()); }

        else if ((proposedDepth = marker->position + halfWidth - currentViewport.right() + marker->deadzone) > 0) {
            marker->depth = proposedDepth;
            marker->drawBitmap = marker->upperDockedBitmap;
            marker->drawRect.setSize(marker->drawBitmap.size());
            marker->drawRect.moveTop(marker->dockPosition.y());
            marker->drawRect.moveRight(currentViewport.width() - 1 - marker->dockPosition.x()); }

        else { marker->depth = 0; marker->drawBitmap = marker->undockedBitmap;
            marker->drawRect.setSize(marker->drawBitmap.size());
            marker->drawRect.moveCenter(QPoint(marker->position - currentViewport.left(), 0));
            marker->drawRect.moveTop(marker->ceiling);
            marker->drawLine.setLine(0, marker->drawBitmap.height() + marker->ceiling, 0, plotAreaRect.height() - 1);
            marker->drawLine.translate(marker->position - currentViewport.left(), 0);
            marker->sensitiveRect = marker->drawRect;
            marker->sensitiveRect.setBottom(plotAreaRect.height() - 1); }

        break;

    case Marker::Left:
    case Marker::Right:

        // This is the vertical case, i.e., the marker marks a position on the vertical axis.
        // The layout is always done as if it were mounted on the left edge, and additional
        // processing is carried out afterwards.

        if ((proposedDepth = marker->position - halfHeight - currentViewport.top() - marker->deadzone) < 0) {
            marker->depth = proposedDepth;
            marker->drawBitmap = marker->lowerDockedBitmap;
            marker->drawRect.setSize(marker->drawBitmap.size());
            marker->drawRect.moveTop(marker->dockPosition.x());
            marker->drawRect.moveLeft(marker->dockPosition.y()); }

        else if ((proposedDepth = marker->position + halfHeight - currentViewport.bottom() + marker->deadzone) > 0) {
            marker->depth = proposedDepth;
            marker->drawBitmap = marker->upperDockedBitmap;
            marker->drawRect.setSize(marker->drawBitmap.size());
            marker->drawRect.moveLeft(marker->dockPosition.y());
            marker->drawRect.moveBottom(currentViewport.height() - 1 - marker->dockPosition.x()); }

        else { marker->depth = 0; marker->drawBitmap = marker->undockedBitmap;
            marker->drawRect.setSize(marker->drawBitmap.size());
            marker->drawRect.moveCenter(QPoint(0, marker->position - currentViewport.top()));
            marker->drawRect.moveLeft(marker->ceiling);
            marker->drawLine.setLine(marker->drawBitmap.width() + marker->ceiling, 0, plotAreaRect.width() - 1, 0);
            marker->drawLine.translate(0, marker->position - currentViewport.top());
            marker->sensitiveRect = marker->drawRect;
            marker->sensitiveRect.setRight(plotAreaRect.width() - 1); }

        break; }

    // This is the aforementioned 'additional processing'. Here we take advantage of the
    // geometric symmetry of the bitmaps, instead of strictly mirroring and translating
    // the marker, we simply move one of its appropriate edge.

    if (marker->mountEdge == Marker::Right) {
        marker->drawRect.moveRight(currentViewport.width() - 1 - marker->drawRect.left());
        marker->drawLine.translate(0 - marker->drawRect.width() - marker->ceiling, 0); }
    if (marker->mountEdge == Marker::Bottom) {
        marker->drawRect.moveBottom(currentViewport.height() - 1 - marker->drawRect.top());
        marker->drawLine.translate(0, 0 - marker->drawRect.height() - marker->ceiling); }

    // If the marker is docked or inactive, do not draw a line, and
    // the mouse-sensitive rect only contains the draw-rect, not the draw-line.

    if (marker->depth != 0 || !marker->isActive) {
        marker->drawLine.setP2(marker->drawLine.p1());
        marker->sensitiveRect = marker->drawRect; }

    marker->drawRect.translate(plotAreaRect.topLeft());
    marker->drawLine.translate(plotAreaRect.topLeft());
    marker->sensitiveRect.translate(plotAreaRect.topLeft());

    // Actually there is still room for further optimization. If the marker is already docked
    // somewhere, and a new round of calculation shows that it should remain where it is, we
    // do not have to re-assign the bitmap and the coordinates again. However, compared to
    // the time saved by avoiding bitmap transformation, this is rather trivial.

    return marker->depth;
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
    // a pre-defined margin to the entire available space, allowing as uxiliary elements such
    // as cursor arrows and markers to dock in between. Note that the tick marks are plotted
    // outside of the plot-area. Also don't forget to update the geometries of the markers.

    plotAreaRect = rect().adjusted(
        0 + plotAreaMarginLeft, 0 + plotAreaMarginTop,
        0 - plotAreaMarginRight, 0 - plotAreaMarginBottom);

    currentViewport.setSize(plotAreaRect.size());
    for (int index = 0; index < cursors.count(); index++)
        updateMarkerGeometry(cursors.value(index));

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
    if (count > 0) { painter.translate(offset); \
        while ((viewportRect.bottomRight() - offset).AXIS() >= 0) { \
            painter.DRAWER(CACHE.data(), count); \
            painter.translate(STEP); offset += STEP; \
        }   painter.translate(-offset); } }

    // event->region.rects are organized in a much fragmented way that is not appropriate
    // for high-efficiency drawing. We could either choose to optimize them, or simply
    // to avoid using save-restore pair such that the additional 'drawing' processes take trivial time.

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

    // ===

    painter.save();
    painter.setBackgroundMode(Qt::OpaqueMode);
    painter.setPen(Qt::yellow);
    painter.setBackground(Qt::black);

    for (int index = 0; index < cursors.count(); index++) {
        Marker *marker = cursors.value(index);
        painter.drawPixmap(marker->drawRect, marker->drawBitmap);
        painter.drawLine(marker->drawLine); }

    painter.restore();

}












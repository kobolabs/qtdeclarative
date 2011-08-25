/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "QtQuick1/private/qdeclarativeflickable_p.h"
#include "QtQuick1/private/qdeclarativeflickable_p_p.h"
#include <QtDeclarative/qdeclarativeinfo.h>
#include <QGraphicsSceneMouseEvent>
#include <QPointer>
#include <QTimer>
#include "qplatformdefs.h"

QT_BEGIN_NAMESPACE



// The maximum number of pixels a flick can overshoot
#ifndef QML_FLICK_OVERSHOOT
#define QML_FLICK_OVERSHOOT 200
#endif

// The number of samples to use in calculating the velocity of a flick
#ifndef QML_FLICK_SAMPLEBUFFER
#define QML_FLICK_SAMPLEBUFFER 3
#endif

// The number of samples to discard when calculating the flick velocity.
// Touch panels often produce inaccurate results as the finger is lifted.
#ifndef QML_FLICK_DISCARDSAMPLES
#define QML_FLICK_DISCARDSAMPLES 1
#endif

// The default maximum velocity of a flick.
#ifndef QML_FLICK_DEFAULTMAXVELOCITY
#define QML_FLICK_DEFAULTMAXVELOCITY 2500
#endif

// The default deceleration of a flick.
#ifndef QML_FLICK_DEFAULTDECELERATION
#define QML_FLICK_DEFAULTDECELERATION 1750
#endif

// How much faster to decelerate when overshooting
#ifndef QML_FLICK_OVERSHOOTFRICTION
#define QML_FLICK_OVERSHOOTFRICTION 8
#endif

// FlickThreshold determines how far the "mouse" must have moved
// before we perform a flick.
static const int FlickThreshold = 20;

// RetainGrabVelocity is the maxmimum instantaneous velocity that
// will ensure the  Flickable retains the grab on consecutive flicks.
static const int RetainGrabVelocity = 15;

QDeclarative1FlickableVisibleArea::QDeclarative1FlickableVisibleArea(QDeclarative1Flickable *parent)
    : QObject(parent), flickable(parent), m_xPosition(0.), m_widthRatio(0.)
    , m_yPosition(0.), m_heightRatio(0.)
{
}

qreal QDeclarative1FlickableVisibleArea::widthRatio() const
{
    return m_widthRatio;
}

qreal QDeclarative1FlickableVisibleArea::xPosition() const
{
    return m_xPosition;
}

qreal QDeclarative1FlickableVisibleArea::heightRatio() const
{
    return m_heightRatio;
}

qreal QDeclarative1FlickableVisibleArea::yPosition() const
{
    return m_yPosition;
}

void QDeclarative1FlickableVisibleArea::updateVisible()
{
    QDeclarative1FlickablePrivate *p = static_cast<QDeclarative1FlickablePrivate *>(QGraphicsItemPrivate::get(flickable));

    bool changeX = false;
    bool changeY = false;
    bool changeWidth = false;
    bool changeHeight = false;

    // Vertical
    const qreal viewheight = flickable->height();
    const qreal maxyextent = -flickable->maxYExtent() + flickable->minYExtent();
    qreal pagePos = (-p->vData.move.value() + flickable->minYExtent()) / (maxyextent + viewheight);
    qreal pageSize = viewheight / (maxyextent + viewheight);

    if (pageSize != m_heightRatio) {
        m_heightRatio = pageSize;
        changeHeight = true;
    }
    if (pagePos != m_yPosition) {
        m_yPosition = pagePos;
        changeY = true;
    }

    // Horizontal
    const qreal viewwidth = flickable->width();
    const qreal maxxextent = -flickable->maxXExtent() + flickable->minXExtent();
    pagePos = (-p->hData.move.value() + flickable->minXExtent()) / (maxxextent + viewwidth);
    pageSize = viewwidth / (maxxextent + viewwidth);

    if (pageSize != m_widthRatio) {
        m_widthRatio = pageSize;
        changeWidth = true;
    }
    if (pagePos != m_xPosition) {
        m_xPosition = pagePos;
        changeX = true;
    }

    if (changeX)
        emit xPositionChanged(m_xPosition);
    if (changeY)
        emit yPositionChanged(m_yPosition);
    if (changeWidth)
        emit widthRatioChanged(m_widthRatio);
    if (changeHeight)
        emit heightRatioChanged(m_heightRatio);
}


QDeclarative1FlickablePrivate::QDeclarative1FlickablePrivate()
  : contentItem(new QDeclarativeItem)
    , hData(this, &QDeclarative1FlickablePrivate::setRoundedViewportX)
    , vData(this, &QDeclarative1FlickablePrivate::setRoundedViewportY)
    , flickingHorizontally(false), flickingVertically(false)
    , hMoved(false), vMoved(false)
    , movingHorizontally(false), movingVertically(false)
    , stealMouse(false), pressed(false), interactive(true), calcVelocity(false)
    , deceleration(QML_FLICK_DEFAULTDECELERATION)
    , maxVelocity(QML_FLICK_DEFAULTMAXVELOCITY), reportedVelocitySmoothing(100)
    , delayedPressEvent(0), delayedPressTarget(0), pressDelay(0), fixupDuration(400)
    , fixupMode(Normal), vTime(0), visibleArea(0)
    , flickableDirection(QDeclarative1Flickable::AutoFlickDirection)
    , boundsBehavior(QDeclarative1Flickable::DragAndOvershootBounds)
{
}

void QDeclarative1FlickablePrivate::init()
{
    Q_Q(QDeclarative1Flickable);
    QDeclarative_setParent_noEvent(contentItem, q);
    contentItem->setParentItem(q);
    static int timelineUpdatedIdx = -1;
    static int timelineCompletedIdx = -1;
    static int flickableTickedIdx = -1;
    static int flickableMovementEndingIdx = -1;
    if (timelineUpdatedIdx == -1) {
        timelineUpdatedIdx = QDeclarative1TimeLine::staticMetaObject.indexOfSignal("updated()");
        timelineCompletedIdx = QDeclarative1TimeLine::staticMetaObject.indexOfSignal("completed()");
        flickableTickedIdx = QDeclarative1Flickable::staticMetaObject.indexOfSlot("ticked()");
        flickableMovementEndingIdx = QDeclarative1Flickable::staticMetaObject.indexOfSlot("movementEnding()");
    }
    QMetaObject::connect(&timeline, timelineUpdatedIdx,
                         q, flickableTickedIdx, Qt::DirectConnection);
    QMetaObject::connect(&timeline, timelineCompletedIdx,
                         q, flickableMovementEndingIdx, Qt::DirectConnection);
    q->setAcceptedMouseButtons(Qt::LeftButton);
    q->setFiltersChildEvents(true);
    QDeclarativeItemPrivate *viewportPrivate = static_cast<QDeclarativeItemPrivate*>(QGraphicsItemPrivate::get(contentItem));
    viewportPrivate->addItemChangeListener(this, QDeclarativeItemPrivate::Geometry);
    lastPosTime.invalidate();
}

/*
    Returns the amount to overshoot by given a view size.
    Will be up to the lesser of 1/3 of the view size or QML_FLICK_OVERSHOOT
*/
qreal QDeclarative1FlickablePrivate::overShootDistance(qreal size)
{
    if (maxVelocity <= 0)
        return 0.0;

    return qMin(qreal(QML_FLICK_OVERSHOOT), size/3);
}

void QDeclarative1FlickablePrivate::AxisData::addVelocitySample(qreal v, qreal maxVelocity)
{
    if (v > maxVelocity)
        v = maxVelocity;
    else if (v < -maxVelocity)
        v = -maxVelocity;
    velocityBuffer.append(v);
    if (velocityBuffer.count() > QML_FLICK_SAMPLEBUFFER)
        velocityBuffer.remove(0);
}

void QDeclarative1FlickablePrivate::AxisData::updateVelocity()
{
    if (velocityBuffer.count() > QML_FLICK_DISCARDSAMPLES) {
        velocity = 0;
        int count = velocityBuffer.count()-QML_FLICK_DISCARDSAMPLES;
        for (int i = 0; i < count; ++i) {
            qreal v = velocityBuffer.at(i);
            velocity += v;
        }
        velocity /= count;
    }
}

void QDeclarative1FlickablePrivate::itemGeometryChanged(QDeclarativeItem *item, const QRectF &newGeom, const QRectF &oldGeom)
{
    Q_Q(QDeclarative1Flickable);
    if (item == contentItem) {
        if (newGeom.x() != oldGeom.x())
            emit q->contentXChanged();
        if (newGeom.y() != oldGeom.y())
            emit q->contentYChanged();
    }
}

void QDeclarative1FlickablePrivate::flickX(qreal velocity)
{
    Q_Q(QDeclarative1Flickable);
    flick(hData, q->minXExtent(), q->maxXExtent(), q->width(), fixupX_callback, velocity);
}

void QDeclarative1FlickablePrivate::flickY(qreal velocity)
{
    Q_Q(QDeclarative1Flickable);
    flick(vData, q->minYExtent(), q->maxYExtent(), q->height(), fixupY_callback, velocity);
}

void QDeclarative1FlickablePrivate::flick(AxisData &data, qreal minExtent, qreal maxExtent, qreal,
                                         QDeclarative1TimeLineCallback::Callback fixupCallback, qreal velocity)
{
    Q_Q(QDeclarative1Flickable);
    qreal maxDistance = -1;
    data.fixingUp = false;
    // -ve velocity means list is moving up
    if (velocity > 0) {
        maxDistance = qAbs(minExtent - data.move.value());
        data.flickTarget = minExtent;
    } else {
        maxDistance = qAbs(maxExtent - data.move.value());
        data.flickTarget = maxExtent;
    }
    if (maxDistance > 0) {
        qreal v = velocity;
        if (maxVelocity != -1 && maxVelocity < qAbs(v)) {
            if (v < 0)
                v = -maxVelocity;
            else
                v = maxVelocity;
        }
        timeline.reset(data.move);
        if (boundsBehavior == QDeclarative1Flickable::DragAndOvershootBounds)
            timeline.accel(data.move, v, deceleration);
        else
            timeline.accel(data.move, v, deceleration, maxDistance);
        timeline.callback(QDeclarative1TimeLineCallback(&data.move, fixupCallback, this));
        if (!flickingHorizontally && q->xflick()) {
            flickingHorizontally = true;
            emit q->flickingChanged();
            emit q->flickingHorizontallyChanged();
            if (!flickingVertically)
                emit q->flickStarted();
        }
        if (!flickingVertically && q->yflick()) {
            flickingVertically = true;
            emit q->flickingChanged();
            emit q->flickingVerticallyChanged();
            if (!flickingHorizontally)
                emit q->flickStarted();
        }
    } else {
        timeline.reset(data.move);
        fixup(data, minExtent, maxExtent);
    }
}

void QDeclarative1FlickablePrivate::fixupY_callback(void *data)
{
    ((QDeclarative1FlickablePrivate *)data)->fixupY();
}

void QDeclarative1FlickablePrivate::fixupX_callback(void *data)
{
    ((QDeclarative1FlickablePrivate *)data)->fixupX();
}

void QDeclarative1FlickablePrivate::fixupX()
{
    Q_Q(QDeclarative1Flickable);
    fixup(hData, q->minXExtent(), q->maxXExtent());
}

void QDeclarative1FlickablePrivate::fixupY()
{
    Q_Q(QDeclarative1Flickable);
    fixup(vData, q->minYExtent(), q->maxYExtent());
}

void QDeclarative1FlickablePrivate::fixup(AxisData &data, qreal minExtent, qreal maxExtent)
{
    if (data.move.value() > minExtent || maxExtent > minExtent) {
        timeline.reset(data.move);
        if (data.move.value() != minExtent) {
            switch (fixupMode) {
            case Immediate:
                timeline.set(data.move, minExtent);
                break;
            case ExtentChanged:
                // The target has changed. Don't start from the beginning; just complete the
                // second half of the animation using the new extent.
                timeline.move(data.move, minExtent, QEasingCurve(QEasingCurve::OutExpo), 3*fixupDuration/4);
                data.fixingUp = true;
                break;
            default: {
                    qreal dist = minExtent - data.move;
                    timeline.move(data.move, minExtent - dist/2, QEasingCurve(QEasingCurve::InQuad), fixupDuration/4);
                    timeline.move(data.move, minExtent, QEasingCurve(QEasingCurve::OutExpo), 3*fixupDuration/4);
                    data.fixingUp = true;
                }
            }
        }
    } else if (data.move.value() < maxExtent) {
        timeline.reset(data.move);
        switch (fixupMode) {
        case Immediate:
            timeline.set(data.move, maxExtent);
            break;
        case ExtentChanged:
            // The target has changed. Don't start from the beginning; just complete the
            // second half of the animation using the new extent.
            timeline.move(data.move, maxExtent, QEasingCurve(QEasingCurve::OutExpo), 3*fixupDuration/4);
            data.fixingUp = true;
            break;
        default: {
                qreal dist = maxExtent - data.move;
                timeline.move(data.move, maxExtent - dist/2, QEasingCurve(QEasingCurve::InQuad), fixupDuration/4);
                timeline.move(data.move, maxExtent, QEasingCurve(QEasingCurve::OutExpo), 3*fixupDuration/4);
                data.fixingUp = true;
            }
        }
    }
    data.inOvershoot = false;
    fixupMode = Normal;
    vTime = timeline.time();
}

void QDeclarative1FlickablePrivate::updateBeginningEnd()
{
    Q_Q(QDeclarative1Flickable);
    bool atBoundaryChange = false;

    // Vertical
    const int maxyextent = int(-q->maxYExtent());
    const qreal ypos = -vData.move.value();
    bool atBeginning = (ypos <= -q->minYExtent());
    bool atEnd = (maxyextent <= ypos);

    if (atBeginning != vData.atBeginning) {
        vData.atBeginning = atBeginning;
        atBoundaryChange = true;
    }
    if (atEnd != vData.atEnd) {
        vData.atEnd = atEnd;
        atBoundaryChange = true;
    }

    // Horizontal
    const int maxxextent = int(-q->maxXExtent());
    const qreal xpos = -hData.move.value();
    atBeginning = (xpos <= -q->minXExtent());
    atEnd = (maxxextent <= xpos);

    if (atBeginning != hData.atBeginning) {
        hData.atBeginning = atBeginning;
        atBoundaryChange = true;
    }
    if (atEnd != hData.atEnd) {
        hData.atEnd = atEnd;
        atBoundaryChange = true;
    }

    if (atBoundaryChange)
        emit q->isAtBoundaryChanged();

    if (visibleArea)
        visibleArea->updateVisible();
}

/*!
    \qmlclass Flickable QDeclarative1Flickable
    \inqmlmodule QtQuick 1
    \since QtQuick 1.0
    \ingroup qml-basic-interaction-elements

    \brief The Flickable item provides a surface that can be "flicked".
    \inherits Item

    The Flickable item places its children on a surface that can be dragged
    and flicked, causing the view onto the child items to scroll. This
    behavior forms the basis of Items that are designed to show large numbers
    of child items, such as \l ListView and \l GridView.

    In traditional user interfaces, views can be scrolled using standard
    controls, such as scroll bars and arrow buttons. In some situations, it
    is also possible to drag the view directly by pressing and holding a
    mouse button while moving the cursor. In touch-based user interfaces,
    this dragging action is often complemented with a flicking action, where
    scrolling continues after the user has stopped touching the view.

    Flickable does not automatically clip its contents. If it is not used as
    a full-screen item, you should consider setting the \l{Item::}{clip} property
    to true.

    \section1 Example Usage

    \div {class="float-right"}
    \inlineimage flickable.gif
    \enddiv

    The following example shows a small view onto a large image in which the
    user can drag or flick the image in order to view different parts of it.

    \snippet doc/src/snippets/qtquick1/flickable.qml document

    \clearfloat

    Items declared as children of a Flickable are automatically parented to the
    Flickable's \l contentItem.  This should be taken into account when
    operating on the children of the Flickable; it is usually the children of
    \c contentItem that are relevant.  For example, the bound of Items added
    to the Flickable will be available by \c contentItem.childrenRect

    \section1 Limitations

    \note Due to an implementation detail, items placed inside a Flickable cannot anchor to it by
    \c id. Use \c parent instead.
*/

/*!
    \qmlsignal QtQuick1::Flickable::onMovementStarted()

    This handler is called when the view begins moving due to user
    interaction.
*/

/*!
    \qmlsignal QtQuick1::Flickable::onMovementEnded()

    This handler is called when the view stops moving due to user
    interaction.  If a flick was generated, this handler will
    be triggered once the flick stops.  If a flick was not
    generated, the handler will be triggered when the
    user stops dragging - i.e. a mouse or touch release.
*/

/*!
    \qmlsignal QtQuick1::Flickable::onFlickStarted()

    This handler is called when the view is flicked.  A flick
    starts from the point that the mouse or touch is released,
    while still in motion.
*/

/*!
    \qmlsignal QtQuick1::Flickable::onFlickEnded()

    This handler is called when the view stops moving due to a flick.
*/

/*!
    \qmlproperty real QtQuick1::Flickable::visibleArea.xPosition
    \qmlproperty real QtQuick1::Flickable::visibleArea.widthRatio
    \qmlproperty real QtQuick1::Flickable::visibleArea.yPosition
    \qmlproperty real QtQuick1::Flickable::visibleArea.heightRatio

    These properties describe the position and size of the currently viewed area.
    The size is defined as the percentage of the full view currently visible,
    scaled to 0.0 - 1.0.  The page position is usually in the range 0.0 (beginning) to
    1.0 minus size ratio (end), i.e. \c yPosition is in the range 0.0 to 1.0-\c heightRatio.
    However, it is possible for the contents to be dragged outside of the normal
    range, resulting in the page positions also being outside the normal range.

    These properties are typically used to draw a scrollbar. For example:

    \snippet doc/src/snippets/qtquick1/flickableScrollbar.qml 0
    \dots 8
    \snippet doc/src/snippets/qtquick1/flickableScrollbar.qml 1

    \sa {declarative/ui-components/scrollbar}{scrollbar example}
*/

QDeclarative1Flickable::QDeclarative1Flickable(QDeclarativeItem *parent)
  : QDeclarativeItem(*(new QDeclarative1FlickablePrivate), parent)
{
    Q_D(QDeclarative1Flickable);
    d->init();
}

QDeclarative1Flickable::QDeclarative1Flickable(QDeclarative1FlickablePrivate &dd, QDeclarativeItem *parent)
  : QDeclarativeItem(dd, parent)
{
    Q_D(QDeclarative1Flickable);
    d->init();
}

QDeclarative1Flickable::~QDeclarative1Flickable()
{
}

/*!
    \qmlproperty real QtQuick1::Flickable::contentX
    \qmlproperty real QtQuick1::Flickable::contentY

    These properties hold the surface coordinate currently at the top-left
    corner of the Flickable. For example, if you flick an image up 100 pixels,
    \c contentY will be 100.
*/
qreal QDeclarative1Flickable::contentX() const
{
    Q_D(const QDeclarative1Flickable);
    return -d->contentItem->x();
}

void QDeclarative1Flickable::setContentX(qreal pos)
{
    Q_D(QDeclarative1Flickable);
    d->timeline.reset(d->hData.move);
    d->vTime = d->timeline.time();
    movementXEnding();
    if (-pos != d->hData.move.value()) {
        d->hData.move.setValue(-pos);
        viewportMoved();
    }
}

qreal QDeclarative1Flickable::contentY() const
{
    Q_D(const QDeclarative1Flickable);
    return -d->contentItem->y();
}

void QDeclarative1Flickable::setContentY(qreal pos)
{
    Q_D(QDeclarative1Flickable);
    d->timeline.reset(d->vData.move);
    d->vTime = d->timeline.time();
    movementYEnding();
    if (-pos != d->vData.move.value()) {
        d->vData.move.setValue(-pos);
        viewportMoved();
    }
}

/*!
    \qmlproperty bool QtQuick1::Flickable::interactive

    This property describes whether the user can interact with the Flickable.
    A user cannot drag or flick a Flickable that is not interactive.

    By default, this property is true.

    This property is useful for temporarily disabling flicking. This allows
    special interaction with Flickable's children; for example, you might want
    to freeze a flickable map while scrolling through a pop-up dialog that
    is a child of the Flickable.
*/
bool QDeclarative1Flickable::isInteractive() const
{
    Q_D(const QDeclarative1Flickable);
    return d->interactive;
}

void QDeclarative1Flickable::setInteractive(bool interactive)
{
    Q_D(QDeclarative1Flickable);
    if (interactive != d->interactive) {
        d->interactive = interactive;
        if (!interactive && (d->flickingHorizontally || d->flickingVertically)) {
            d->timeline.clear();
            d->vTime = d->timeline.time();
            d->flickingHorizontally = false;
            d->flickingVertically = false;
            emit flickingChanged();
            emit flickingHorizontallyChanged();
            emit flickingVerticallyChanged();
            emit flickEnded();
        }
        emit interactiveChanged();
    }
}

/*!
    \qmlproperty real QtQuick1::Flickable::horizontalVelocity
    \qmlproperty real QtQuick1::Flickable::verticalVelocity

    The instantaneous velocity of movement along the x and y axes, in pixels/sec.

    The reported velocity is smoothed to avoid erratic output.
*/
qreal QDeclarative1Flickable::horizontalVelocity() const
{
    Q_D(const QDeclarative1Flickable);
    return d->hData.smoothVelocity.value();
}

qreal QDeclarative1Flickable::verticalVelocity() const
{
    Q_D(const QDeclarative1Flickable);
    return d->vData.smoothVelocity.value();
}

/*!
    \qmlproperty bool QtQuick1::Flickable::atXBeginning
    \qmlproperty bool QtQuick1::Flickable::atXEnd
    \qmlproperty bool QtQuick1::Flickable::atYBeginning
    \qmlproperty bool QtQuick1::Flickable::atYEnd

    These properties are true if the flickable view is positioned at the beginning,
    or end respecively.
*/
bool QDeclarative1Flickable::isAtXEnd() const
{
    Q_D(const QDeclarative1Flickable);
    return d->hData.atEnd;
}

bool QDeclarative1Flickable::isAtXBeginning() const
{
    Q_D(const QDeclarative1Flickable);
    return d->hData.atBeginning;
}

bool QDeclarative1Flickable::isAtYEnd() const
{
    Q_D(const QDeclarative1Flickable);
    return d->vData.atEnd;
}

bool QDeclarative1Flickable::isAtYBeginning() const
{
    Q_D(const QDeclarative1Flickable);
    return d->vData.atBeginning;
}

void QDeclarative1Flickable::ticked()
{
    viewportMoved();
}

/*!
    \qmlproperty Item QtQuick1::Flickable::contentItem

    The internal item that contains the Items to be moved in the Flickable.

    Items declared as children of a Flickable are automatically parented to the Flickable's contentItem.

    Items created dynamically need to be explicitly parented to the \e contentItem:
    \code
    Flickable {
        id: myFlickable
        function addItem(file) {
            var component = Qt.createComponent(file)
            component.createObject(myFlickable.contentItem);
        }
    }
    \endcode
*/
QDeclarativeItem *QDeclarative1Flickable::contentItem()
{
    Q_D(QDeclarative1Flickable);
    return d->contentItem;
}

QDeclarative1FlickableVisibleArea *QDeclarative1Flickable::visibleArea()
{
    Q_D(QDeclarative1Flickable);
    if (!d->visibleArea)
        d->visibleArea = new QDeclarative1FlickableVisibleArea(this);
    return d->visibleArea;
}

/*!
    \qmlproperty enumeration QtQuick1::Flickable::flickableDirection

    This property determines which directions the view can be flicked.

    \list
    \o Flickable.AutoFlickDirection (default) - allows flicking vertically if the
    \e contentHeight is not equal to the \e height of the Flickable.
    Allows flicking horizontally if the \e contentWidth is not equal
    to the \e width of the Flickable.
    \o Flickable.HorizontalFlick - allows flicking horizontally.
    \o Flickable.VerticalFlick - allows flicking vertically.
    \o Flickable.HorizontalAndVerticalFlick - allows flicking in both directions.
    \endlist
*/
QDeclarative1Flickable::FlickableDirection QDeclarative1Flickable::flickableDirection() const
{
    Q_D(const QDeclarative1Flickable);
    return d->flickableDirection;
}

void QDeclarative1Flickable::setFlickableDirection(FlickableDirection direction)
{
    Q_D(QDeclarative1Flickable);
    if (direction != d->flickableDirection) {
        d->flickableDirection = direction;
        emit flickableDirectionChanged();
    }
}

void QDeclarative1FlickablePrivate::handleMousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_Q(QDeclarative1Flickable);
    if (interactive && timeline.isActive()
            && (qAbs(hData.smoothVelocity.value()) > RetainGrabVelocity || qAbs(vData.smoothVelocity.value()) > RetainGrabVelocity))
        stealMouse = true; // If we've been flicked then steal the click.
    else
        stealMouse = false;
    q->setKeepMouseGrab(stealMouse);
    pressed = true;
    timeline.clear();
    hData.reset();
    vData.reset();
    hData.dragMinBound = q->minXExtent();
    vData.dragMinBound = q->minYExtent();
    hData.dragMaxBound = q->maxXExtent();
    vData.dragMaxBound = q->maxYExtent();
    fixupMode = Normal;
    lastPos = QPoint();
    QDeclarativeItemPrivate::start(lastPosTime);
    pressPos = event->pos();
    hData.pressPos = hData.move.value();
    vData.pressPos = vData.move.value();
    flickingHorizontally = false;
    flickingVertically = false;
    QDeclarativeItemPrivate::start(pressTime);
    QDeclarativeItemPrivate::start(velocityTime);
}

void QDeclarative1FlickablePrivate::handleMouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_Q(QDeclarative1Flickable);
    if (!interactive || !lastPosTime.isValid())
        return;
    bool rejectY = false;
    bool rejectX = false;

    bool stealY = stealMouse;
    bool stealX = stealMouse;

    if (q->yflick()) {
        int dy = int(event->pos().y() - pressPos.y());
        if (qAbs(dy) > QApplication::startDragDistance() || QDeclarativeItemPrivate::elapsed(pressTime) > 200) {
            if (!vMoved)
                vData.dragStartOffset = dy;
            qreal newY = dy + vData.pressPos - vData.dragStartOffset;
            const qreal minY = vData.dragMinBound;
            const qreal maxY = vData.dragMaxBound;
            if (newY > minY)
                newY = minY + (newY - minY) / 2;
            if (newY < maxY && maxY - minY <= 0)
                newY = maxY + (newY - maxY) / 2;
            if (boundsBehavior == QDeclarative1Flickable::StopAtBounds && (newY > minY || newY < maxY)) {
                rejectY = true;
                if (newY < maxY) {
                    newY = maxY;
                    rejectY = false;
                }
                if (newY > minY) {
                    newY = minY;
                    rejectY = false;
                }
            }
            if (!rejectY && stealMouse) {
                vData.move.setValue(qRound(newY));
                vMoved = true;
            }
            if (qAbs(dy) > QApplication::startDragDistance())
                stealY = true;
        }
    }

    if (q->xflick()) {
        int dx = int(event->pos().x() - pressPos.x());
        if (qAbs(dx) > QApplication::startDragDistance() || QDeclarativeItemPrivate::elapsed(pressTime) > 200) {
            if (!hMoved)
                hData.dragStartOffset = dx;
            qreal newX = dx + hData.pressPos - hData.dragStartOffset;
            const qreal minX = hData.dragMinBound;
            const qreal maxX = hData.dragMaxBound;
            if (newX > minX)
                newX = minX + (newX - minX) / 2;
            if (newX < maxX && maxX - minX <= 0)
                newX = maxX + (newX - maxX) / 2;
            if (boundsBehavior == QDeclarative1Flickable::StopAtBounds && (newX > minX || newX < maxX)) {
                rejectX = true;
                if (newX < maxX) {
                    newX = maxX;
                    rejectX = false;
                }
                if (newX > minX) {
                    newX = minX;
                    rejectX = false;
                }
            }
            if (!rejectX && stealMouse) {
                hData.move.setValue(qRound(newX));
                hMoved = true;
            }

            if (qAbs(dx) > QApplication::startDragDistance())
                stealX = true;
        }
    }

    stealMouse = stealX || stealY;
    if (stealMouse)
        q->setKeepMouseGrab(true);

    if (rejectY) {
        vData.velocityBuffer.clear();
        vData.velocity = 0;
    }
    if (rejectX) {
        hData.velocityBuffer.clear();
        hData.velocity = 0;
    }

    if (hMoved || vMoved) {
        q->movementStarting();
        q->viewportMoved();
    }

    if (!lastPos.isNull()) {
        qreal elapsed = qreal(QDeclarativeItemPrivate::elapsed(lastPosTime)) / 1000.;
        if (elapsed <= 0)
            return;
        QDeclarativeItemPrivate::restart(lastPosTime);
        qreal dy = event->pos().y()-lastPos.y();
        if (q->yflick() && !rejectY)
            vData.addVelocitySample(dy/elapsed, maxVelocity);
        qreal dx = event->pos().x()-lastPos.x();
        if (q->xflick() && !rejectX)
            hData.addVelocitySample(dx/elapsed, maxVelocity);
    }

    lastPos = event->pos();
}

void QDeclarative1FlickablePrivate::handleMouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_Q(QDeclarative1Flickable);
    stealMouse = false;
    q->setKeepMouseGrab(false);
    pressed = false;
    if (!lastPosTime.isValid())
        return;

    // if we drag then pause before release we should not cause a flick.
    if (QDeclarativeItemPrivate::elapsed(lastPosTime) < 100) {
        vData.updateVelocity();
        hData.updateVelocity();
    } else {
        hData.velocity = 0.0;
        vData.velocity = 0.0;
    }

    vTime = timeline.time();

    qreal velocity = vData.velocity;
    if (vData.atBeginning || vData.atEnd)
        velocity /= 2;
    if (qAbs(velocity) > MinimumFlickVelocity && qAbs(event->pos().y() - pressPos.y()) > FlickThreshold)
        flickY(velocity);
    else
        fixupY();

    velocity = hData.velocity;
    if (hData.atBeginning || hData.atEnd)
        velocity /= 2;
    if (qAbs(velocity) > MinimumFlickVelocity && qAbs(event->pos().x() - pressPos.x()) > FlickThreshold)
        flickX(velocity);
    else
        fixupX();

    if (!timeline.isActive())
        q->movementEnding();
}

void QDeclarative1Flickable::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(QDeclarative1Flickable);
    if (d->interactive) {
        if (!d->pressed)
            d->handleMousePressEvent(event);
        event->accept();
    } else {
        QDeclarativeItem::mousePressEvent(event);
    }
}

void QDeclarative1Flickable::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(QDeclarative1Flickable);
    if (d->interactive) {
        d->handleMouseMoveEvent(event);
        event->accept();
    } else {
        QDeclarativeItem::mouseMoveEvent(event);
    }
}

void QDeclarative1Flickable::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(QDeclarative1Flickable);
    if (d->interactive) {
        d->clearDelayedPress();
        d->handleMouseReleaseEvent(event);
        event->accept();
        ungrabMouse();
    } else {
        QDeclarativeItem::mouseReleaseEvent(event);
    }
}

void QDeclarative1Flickable::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    Q_D(QDeclarative1Flickable);
    if (!d->interactive) {
        QDeclarativeItem::wheelEvent(event);
    } else if (yflick() && event->orientation() == Qt::Vertical) {
        bool valid = false;
        if (event->delta() > 0 && contentY() > -minYExtent()) {
            d->vData.velocity = qMax(event->delta()*2 - d->vData.smoothVelocity.value(), qreal(d->maxVelocity/4));
            valid = true;
        } else if (event->delta() < 0 && contentY() < -maxYExtent()) {
            d->vData.velocity = qMin(event->delta()*2 - d->vData.smoothVelocity.value(), qreal(-d->maxVelocity/4));
            valid = true;
        }
        if (valid) {
            d->flickingVertically = false;
            d->flickY(d->vData.velocity);
            if (d->flickingVertically) {
                d->vMoved = true;
                movementStarting();
            }
            event->accept();
        }
    } else if (xflick() && event->orientation() == Qt::Horizontal) {
        bool valid = false;
        if (event->delta() > 0 && contentX() > -minXExtent()) {
            d->hData.velocity = qMax(event->delta()*2 - d->hData.smoothVelocity.value(), qreal(d->maxVelocity/4));
            valid = true;
        } else if (event->delta() < 0 && contentX() < -maxXExtent()) {
            d->hData.velocity = qMin(event->delta()*2 - d->hData.smoothVelocity.value(), qreal(-d->maxVelocity/4));
            valid = true;
        }
        if (valid) {
            d->flickingHorizontally = false;
            d->flickX(d->hData.velocity);
            if (d->flickingHorizontally) {
                d->hMoved = true;
                movementStarting();
            }
            event->accept();
        }
    } else {
        QDeclarativeItem::wheelEvent(event);
    }
}

bool QDeclarative1FlickablePrivate::isOutermostPressDelay() const
{
    Q_Q(const QDeclarative1Flickable);
    QDeclarativeItem *item = q->parentItem();
    while (item) {
        QDeclarative1Flickable *flick = qobject_cast<QDeclarative1Flickable*>(item);
        if (flick && flick->pressDelay() > 0 && flick->isInteractive())
            return false;
        item = item->parentItem();
    }

    return true;
}

void QDeclarative1FlickablePrivate::captureDelayedPress(QGraphicsSceneMouseEvent *event)
{
    Q_Q(QDeclarative1Flickable);
    if (!q->scene() || pressDelay <= 0)
        return;
    if (!isOutermostPressDelay())
        return;
    delayedPressTarget = q->scene()->mouseGrabberItem();
    delayedPressEvent = new QGraphicsSceneMouseEvent(event->type());
    delayedPressEvent->setAccepted(false);
    for (int i = 0x1; i <= 0x10; i <<= 1) {
        if (event->buttons() & i) {
            Qt::MouseButton button = Qt::MouseButton(i);
            delayedPressEvent->setButtonDownPos(button, event->buttonDownPos(button));
            delayedPressEvent->setButtonDownScenePos(button, event->buttonDownScenePos(button));
            delayedPressEvent->setButtonDownScreenPos(button, event->buttonDownScreenPos(button));
        }
    }
    delayedPressEvent->setButtons(event->buttons());
    delayedPressEvent->setButton(event->button());
    delayedPressEvent->setPos(event->pos());
    delayedPressEvent->setScenePos(event->scenePos());
    delayedPressEvent->setScreenPos(event->screenPos());
    delayedPressEvent->setLastPos(event->lastPos());
    delayedPressEvent->setLastScenePos(event->lastScenePos());
    delayedPressEvent->setLastScreenPos(event->lastScreenPos());
    delayedPressEvent->setModifiers(event->modifiers());
    delayedPressTimer.start(pressDelay, q);
}

void QDeclarative1FlickablePrivate::clearDelayedPress()
{
    if (delayedPressEvent) {
        delayedPressTimer.stop();
        delete delayedPressEvent;
        delayedPressEvent = 0;
    }
}

void QDeclarative1FlickablePrivate::setRoundedViewportX(qreal x)
{
    contentItem->setX(qRound(x));
}

void QDeclarative1FlickablePrivate::setRoundedViewportY(qreal y)
{
    contentItem->setY(qRound(y));
}

void QDeclarative1Flickable::timerEvent(QTimerEvent *event)
{
    Q_D(QDeclarative1Flickable);
    if (event->timerId() == d->delayedPressTimer.timerId()) {
        d->delayedPressTimer.stop();
        if (d->delayedPressEvent) {
            QDeclarativeItem *grabber = scene() ? qobject_cast<QDeclarativeItem*>(scene()->mouseGrabberItem()) : 0;
            if (!grabber || grabber != this) {
                // We replay the mouse press but the grabber we had might not be interessted by the event (e.g. overlay)
                // so we reset the grabber
                if (scene()->mouseGrabberItem() == d->delayedPressTarget)
                    d->delayedPressTarget->ungrabMouse();
                //Use the event handler that will take care of finding the proper item to propagate the event
                QApplication::postEvent(scene(), d->delayedPressEvent);
            } else {
                delete d->delayedPressEvent;
            }
            d->delayedPressEvent = 0;
        }
    }
}

qreal QDeclarative1Flickable::minYExtent() const
{
    return 0.0;
}

qreal QDeclarative1Flickable::minXExtent() const
{
    return 0.0;
}

/* returns -ve */
qreal QDeclarative1Flickable::maxXExtent() const
{
    return width() - vWidth();
}
/* returns -ve */
qreal QDeclarative1Flickable::maxYExtent() const
{
    return height() - vHeight();
}

void QDeclarative1Flickable::viewportMoved()
{
    Q_D(QDeclarative1Flickable);

    qreal prevX = d->lastFlickablePosition.x();
    qreal prevY = d->lastFlickablePosition.y();
    d->velocityTimeline.clear();
    if (d->pressed || d->calcVelocity) {
        int elapsed = QDeclarativeItemPrivate::restart(d->velocityTime);
        if (elapsed > 0) {
            qreal horizontalVelocity = (prevX - d->hData.move.value()) * 1000 / elapsed;
            qreal verticalVelocity = (prevY - d->vData.move.value()) * 1000 / elapsed;
            d->velocityTimeline.move(d->hData.smoothVelocity, horizontalVelocity, d->reportedVelocitySmoothing);
            d->velocityTimeline.move(d->hData.smoothVelocity, 0, d->reportedVelocitySmoothing);
            d->velocityTimeline.move(d->vData.smoothVelocity, verticalVelocity, d->reportedVelocitySmoothing);
            d->velocityTimeline.move(d->vData.smoothVelocity, 0, d->reportedVelocitySmoothing);
        }
    } else {
        if (d->timeline.time() > d->vTime) {
            qreal horizontalVelocity = (prevX - d->hData.move.value()) * 1000 / (d->timeline.time() - d->vTime);
            qreal verticalVelocity = (prevY - d->vData.move.value()) * 1000 / (d->timeline.time() - d->vTime);
            d->hData.smoothVelocity.setValue(horizontalVelocity);
            d->vData.smoothVelocity.setValue(verticalVelocity);
        }
    }

    if (!d->vData.inOvershoot && !d->vData.fixingUp && d->flickingVertically
            && (d->vData.move.value() > minYExtent() || d->vData.move.value() < maxYExtent())
            && qAbs(d->vData.smoothVelocity.value()) > 100) {
        // Increase deceleration if we've passed a bound
        d->vData.inOvershoot = true;
        qreal maxDistance = d->overShootDistance(height());
        d->timeline.reset(d->vData.move);
        d->timeline.accel(d->vData.move, -d->vData.smoothVelocity.value(), d->deceleration*QML_FLICK_OVERSHOOTFRICTION, maxDistance);
        d->timeline.callback(QDeclarative1TimeLineCallback(&d->vData.move, d->fixupY_callback, d));
    }
    if (!d->hData.inOvershoot && !d->hData.fixingUp && d->flickingHorizontally
            && (d->hData.move.value() > minXExtent() || d->hData.move.value() < maxXExtent())
            && qAbs(d->hData.smoothVelocity.value()) > 100) {
        // Increase deceleration if we've passed a bound
        d->hData.inOvershoot = true;
        qreal maxDistance = d->overShootDistance(width());
        d->timeline.reset(d->hData.move);
        d->timeline.accel(d->hData.move, -d->hData.smoothVelocity.value(), d->deceleration*QML_FLICK_OVERSHOOTFRICTION, maxDistance);
        d->timeline.callback(QDeclarative1TimeLineCallback(&d->hData.move, d->fixupX_callback, d));
    }

    d->lastFlickablePosition = QPointF(d->hData.move.value(), d->vData.move.value());

    d->vTime = d->timeline.time();
    d->updateBeginningEnd();
}

void QDeclarative1Flickable::geometryChanged(const QRectF &newGeometry,
                             const QRectF &oldGeometry)
{
    Q_D(QDeclarative1Flickable);
    QDeclarativeItem::geometryChanged(newGeometry, oldGeometry);

    bool changed = false;
    if (newGeometry.width() != oldGeometry.width()) {
        if (xflick())
            changed = true;
        if (d->hData.viewSize < 0) {
            d->contentItem->setWidth(width());
            emit contentWidthChanged();
        }
        // Make sure that we're entirely in view.
        if (!d->pressed && !d->movingHorizontally && !d->movingVertically) {
            d->fixupMode = QDeclarative1FlickablePrivate::Immediate;
            d->fixupX();
        }
    }
    if (newGeometry.height() != oldGeometry.height()) {
        if (yflick())
            changed = true;
        if (d->vData.viewSize < 0) {
            d->contentItem->setHeight(height());
            emit contentHeightChanged();
        }
        // Make sure that we're entirely in view.
        if (!d->pressed && !d->movingHorizontally && !d->movingVertically) {
            d->fixupMode = QDeclarative1FlickablePrivate::Immediate;
            d->fixupY();
        }
    }

    if (changed)
        d->updateBeginningEnd();
}

void QDeclarative1Flickable::cancelFlick()
{
    Q_D(QDeclarative1Flickable);
    d->timeline.reset(d->hData.move);
    d->timeline.reset(d->vData.move);
    movementEnding();
}

void QDeclarative1FlickablePrivate::data_append(QDeclarativeListProperty<QObject> *prop, QObject *o)
{
    QGraphicsObject *i = qobject_cast<QGraphicsObject *>(o);
    if (i) {
        QGraphicsItemPrivate *d = QGraphicsItemPrivate::get(i);
        if (static_cast<QDeclarativeItemPrivate*>(d)->componentComplete) {
            i->setParentItem(static_cast<QDeclarative1FlickablePrivate*>(prop->data)->contentItem);
        } else {
            d->setParentItemHelper(static_cast<QDeclarative1FlickablePrivate*>(prop->data)->contentItem, 0, 0);
        }
    } else {
        o->setParent(prop->object);
    }
}

int QDeclarative1FlickablePrivate::data_count(QDeclarativeListProperty<QObject> *property)
{
    QDeclarativeItem *contentItem= static_cast<QDeclarative1FlickablePrivate*>(property->data)->contentItem;
    return contentItem->childItems().count() + contentItem->children().count();
}

QObject *QDeclarative1FlickablePrivate::data_at(QDeclarativeListProperty<QObject> *property, int index)
{
    QDeclarativeItem *contentItem = static_cast<QDeclarative1FlickablePrivate*>(property->data)->contentItem;

    int childItemCount = contentItem->childItems().count();

    if (index < 0)
        return 0;

    if (index < childItemCount) {
        return contentItem->childItems().at(index)->toGraphicsObject();
    } else {
        return contentItem->children().at(index - childItemCount);
    }

    return 0;
}

void QDeclarative1FlickablePrivate::data_clear(QDeclarativeListProperty<QObject> *property)
{
    QDeclarativeItem *contentItem = static_cast<QDeclarative1FlickablePrivate*>(property->data)->contentItem;

    const QList<QGraphicsItem*> graphicsItems = contentItem->childItems();
    for (int i = 0; i < graphicsItems.count(); i++)
        contentItem->scene()->removeItem(graphicsItems[i]);

    const QList<QObject*> objects = contentItem->children();
    for (int i = 0; i < objects.count(); i++)
        objects[i]->setParent(0);
}

QDeclarativeListProperty<QObject> QDeclarative1Flickable::flickableData()
{
    Q_D(QDeclarative1Flickable);
    return QDeclarativeListProperty<QObject>(this, (void *)d, QDeclarative1FlickablePrivate::data_append,
                                                              QDeclarative1FlickablePrivate::data_count,
                                                              QDeclarative1FlickablePrivate::data_at,
                                                              QDeclarative1FlickablePrivate::data_clear);
}

QDeclarativeListProperty<QGraphicsObject> QDeclarative1Flickable::flickableChildren()
{
    Q_D(QDeclarative1Flickable);
    return QGraphicsItemPrivate::get(d->contentItem)->childrenList();
}

/*!
    \qmlproperty enumeration QtQuick1::Flickable::boundsBehavior
    This property holds whether the surface may be dragged
    beyond the Fickable's boundaries, or overshoot the
    Flickable's boundaries when flicked.

    This enables the feeling that the edges of the view are soft,
    rather than a hard physical boundary.

    The \c boundsBehavior can be one of:

    \list
    \o Flickable.StopAtBounds - the contents can not be dragged beyond the boundary
    of the flickable, and flicks will not overshoot.
    \o Flickable.DragOverBounds - the contents can be dragged beyond the boundary
    of the Flickable, but flicks will not overshoot.
    \o Flickable.DragAndOvershootBounds (default) - the contents can be dragged
    beyond the boundary of the Flickable, and can overshoot the
    boundary when flicked.
    \endlist
*/
QDeclarative1Flickable::BoundsBehavior QDeclarative1Flickable::boundsBehavior() const
{
    Q_D(const QDeclarative1Flickable);
    return d->boundsBehavior;
}

void QDeclarative1Flickable::setBoundsBehavior(BoundsBehavior b)
{
    Q_D(QDeclarative1Flickable);
    if (b == d->boundsBehavior)
        return;
    d->boundsBehavior = b;
    emit boundsBehaviorChanged();
}

/*!
    \qmlproperty real QtQuick1::Flickable::contentWidth
    \qmlproperty real QtQuick1::Flickable::contentHeight

    The dimensions of the content (the surface controlled by Flickable).
    This should typically be set to the combined size of the items placed in the
    Flickable.

    The following snippet shows how these properties are used to display
    an image that is larger than the Flickable item itself:

    \snippet doc/src/snippets/qtquick1/flickable.qml document

    In some cases, the the content dimensions can be automatically set
    using the \l {Item::childrenRect.width}{childrenRect.width}
    and \l {Item::childrenRect.height}{childrenRect.height} properties.
*/
qreal QDeclarative1Flickable::contentWidth() const
{
    Q_D(const QDeclarative1Flickable);
    return d->hData.viewSize;
}

void QDeclarative1Flickable::setContentWidth(qreal w)
{
    Q_D(QDeclarative1Flickable);
    if (d->hData.viewSize == w)
        return;
    d->hData.viewSize = w;
    if (w < 0)
        d->contentItem->setWidth(width());
    else
        d->contentItem->setWidth(w);
    // Make sure that we're entirely in view.
    if (!d->pressed && !d->movingHorizontally && !d->movingVertically) {
        d->fixupMode = QDeclarative1FlickablePrivate::Immediate;
        d->fixupX();
    } else if (!d->pressed && d->hData.fixingUp) {
        d->fixupMode = QDeclarative1FlickablePrivate::ExtentChanged;
        d->fixupX();
    }
    emit contentWidthChanged();
    d->updateBeginningEnd();
}

qreal QDeclarative1Flickable::contentHeight() const
{
    Q_D(const QDeclarative1Flickable);
    return d->vData.viewSize;
}

void QDeclarative1Flickable::setContentHeight(qreal h)
{
    Q_D(QDeclarative1Flickable);
    if (d->vData.viewSize == h)
        return;
    d->vData.viewSize = h;
    if (h < 0)
        d->contentItem->setHeight(height());
    else
        d->contentItem->setHeight(h);
    // Make sure that we're entirely in view.
    if (!d->pressed && !d->movingHorizontally && !d->movingVertically) {
        d->fixupMode = QDeclarative1FlickablePrivate::Immediate;
        d->fixupY();
    } else if (!d->pressed && d->vData.fixingUp) {
        d->fixupMode = QDeclarative1FlickablePrivate::ExtentChanged;
        d->fixupY();
    }
    emit contentHeightChanged();
    d->updateBeginningEnd();
}

/*!
    \qmlmethod QtQuick1::Flickable::resizeContent(real width, real height, QPointF center)
    \preliminary
    \since Quick 1.1

    Resizes the content to \a width x \a height about \a center.

    This does not scale the contents of the Flickable - it only resizes the \l contentWidth
    and \l contentHeight.

    Resizing the content may result in the content being positioned outside
    the bounds of the Flickable.  Calling \l returnToBounds() will
    move the content back within legal bounds.
*/
void QDeclarative1Flickable::resizeContent(qreal w, qreal h, QPointF center)
{
    Q_D(QDeclarative1Flickable);
    if (w != d->hData.viewSize) {
        qreal oldSize = d->hData.viewSize;
        d->hData.viewSize = w;
        d->contentItem->setWidth(w);
        emit contentWidthChanged();
        if (center.x() != 0) {
            qreal pos = center.x() * w / oldSize;
            setContentX(contentX() + pos - center.x());
        }
    }
    if (h != d->vData.viewSize) {
        qreal oldSize = d->vData.viewSize;
        d->vData.viewSize = h;
        d->contentItem->setHeight(h);
        emit contentHeightChanged();
        if (center.y() != 0) {
            qreal pos = center.y() * h / oldSize;
            setContentY(contentY() + pos - center.y());
        }
    }
    d->updateBeginningEnd();
}

/*!
    \qmlmethod QtQuick1::Flickable::returnToBounds()
    \preliminary
    \since Quick 1.1

    Ensures the content is within legal bounds.

    This may be called to ensure that the content is within legal bounds
    after manually positioning the content.
*/
void QDeclarative1Flickable::returnToBounds()
{
    Q_D(QDeclarative1Flickable);
    d->fixupX();
    d->fixupY();
}

qreal QDeclarative1Flickable::vWidth() const
{
    Q_D(const QDeclarative1Flickable);
    if (d->hData.viewSize < 0)
        return width();
    else
        return d->hData.viewSize;
}

qreal QDeclarative1Flickable::vHeight() const
{
    Q_D(const QDeclarative1Flickable);
    if (d->vData.viewSize < 0)
        return height();
    else
        return d->vData.viewSize;
}

bool QDeclarative1Flickable::xflick() const
{
    Q_D(const QDeclarative1Flickable);
    if (d->flickableDirection == QDeclarative1Flickable::AutoFlickDirection)
        return vWidth() != width();
    return d->flickableDirection & QDeclarative1Flickable::HorizontalFlick;
}

bool QDeclarative1Flickable::yflick() const
{
    Q_D(const QDeclarative1Flickable);
    if (d->flickableDirection == QDeclarative1Flickable::AutoFlickDirection)
        return vHeight() !=  height();
    return d->flickableDirection & QDeclarative1Flickable::VerticalFlick;
}

bool QDeclarative1Flickable::sceneEvent(QEvent *event)
{
    bool rv = QDeclarativeItem::sceneEvent(event);
    if (event->type() == QEvent::UngrabMouse) {
        Q_D(QDeclarative1Flickable);
        if (d->pressed) {
            // if our mouse grab has been removed (probably by another Flickable),
            // fix our state
            d->pressed = false;
            d->stealMouse = false;
            setKeepMouseGrab(false);
        }
    }
    return rv;
}

bool QDeclarative1Flickable::sendMouseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(QDeclarative1Flickable);
    QGraphicsSceneMouseEvent mouseEvent(event->type());
    QRectF myRect = mapToScene(QRectF(0, 0, width(), height())).boundingRect();

    QGraphicsScene *s = scene();
    QDeclarativeItem *grabber = s ? qobject_cast<QDeclarativeItem*>(s->mouseGrabberItem()) : 0;
    QGraphicsItem *grabberItem = s ? s->mouseGrabberItem() : 0;
    bool disabledItem = grabberItem && !grabberItem->isEnabled();
    bool stealThisEvent = d->stealMouse;
    if ((stealThisEvent || myRect.contains(event->scenePos().toPoint())) && (!grabber || !grabber->keepMouseGrab() || disabledItem)) {
        mouseEvent.setAccepted(false);
        for (int i = 0x1; i <= 0x10; i <<= 1) {
            if (event->buttons() & i) {
                Qt::MouseButton button = Qt::MouseButton(i);
                mouseEvent.setButtonDownPos(button, mapFromScene(event->buttonDownPos(button)));
            }
        }
        mouseEvent.setScenePos(event->scenePos());
        mouseEvent.setLastScenePos(event->lastScenePos());
        mouseEvent.setPos(mapFromScene(event->scenePos()));
        mouseEvent.setLastPos(mapFromScene(event->lastScenePos()));

        switch(mouseEvent.type()) {
        case QEvent::GraphicsSceneMouseMove:
            d->handleMouseMoveEvent(&mouseEvent);
            break;
        case QEvent::GraphicsSceneMousePress:
            if (d->pressed) // we are already pressed - this is a delayed replay
                return false;

            d->handleMousePressEvent(&mouseEvent);
            d->captureDelayedPress(event);
            stealThisEvent = d->stealMouse;   // Update stealThisEvent in case changed by function call above
            break;
        case QEvent::GraphicsSceneMouseRelease:
            if (d->delayedPressEvent) {
                // We replay the mouse press but the grabber we had might not be interessted by the event (e.g. overlay)
                // so we reset the grabber
                if (s->mouseGrabberItem() == d->delayedPressTarget)
                    d->delayedPressTarget->ungrabMouse();
                //Use the event handler that will take care of finding the proper item to propagate the event
                QApplication::sendEvent(scene(), d->delayedPressEvent);
                d->clearDelayedPress();
                // We send the release
                scene()->sendEvent(s->mouseGrabberItem(), event);
                // And the event has been consumed
                d->stealMouse = false;
                d->pressed = false;
                return true;
            }
            d->handleMouseReleaseEvent(&mouseEvent);
            break;
        default:
            break;
        }
        grabber = qobject_cast<QDeclarativeItem*>(s->mouseGrabberItem());
        if ((grabber && stealThisEvent && !grabber->keepMouseGrab() && grabber != this) || disabledItem) {
            d->clearDelayedPress();
            grabMouse();
        }

        return stealThisEvent || d->delayedPressEvent || disabledItem;
    } else if (d->lastPosTime.isValid()) {
        d->lastPosTime.invalidate();
        returnToBounds();
    }
    if (mouseEvent.type() == QEvent::GraphicsSceneMouseRelease) {
        d->clearDelayedPress();
        d->stealMouse = false;
        d->pressed = false;
    }

    return false;
}

bool QDeclarative1Flickable::sceneEventFilter(QGraphicsItem *i, QEvent *e)
{
    Q_D(QDeclarative1Flickable);
    if (!isVisible() || !d->interactive || !isEnabled())
        return QDeclarativeItem::sceneEventFilter(i, e);
    switch (e->type()) {
    case QEvent::GraphicsSceneMousePress:
    case QEvent::GraphicsSceneMouseMove:
    case QEvent::GraphicsSceneMouseRelease:
        return sendMouseEvent(static_cast<QGraphicsSceneMouseEvent *>(e));
    default:
        break;
    }

    return QDeclarativeItem::sceneEventFilter(i, e);
}

/*!
    \qmlproperty real QtQuick1::Flickable::maximumFlickVelocity
    This property holds the maximum velocity that the user can flick the view in pixels/second.

    The default value is platform dependent.
*/
qreal QDeclarative1Flickable::maximumFlickVelocity() const
{
    Q_D(const QDeclarative1Flickable);
    return d->maxVelocity;
}

void QDeclarative1Flickable::setMaximumFlickVelocity(qreal v)
{
    Q_D(QDeclarative1Flickable);
    if (v == d->maxVelocity)
        return;
    d->maxVelocity = v;
    emit maximumFlickVelocityChanged();
}

/*!
    \qmlproperty real QtQuick1::Flickable::flickDeceleration
    This property holds the rate at which a flick will decelerate.

    The default value is platform dependent.
*/
qreal QDeclarative1Flickable::flickDeceleration() const
{
    Q_D(const QDeclarative1Flickable);
    return d->deceleration;
}

void QDeclarative1Flickable::setFlickDeceleration(qreal deceleration)
{
    Q_D(QDeclarative1Flickable);
    if (deceleration == d->deceleration)
        return;
    d->deceleration = deceleration;
    emit flickDecelerationChanged();
}

bool QDeclarative1Flickable::isFlicking() const
{
    Q_D(const QDeclarative1Flickable);
    return d->flickingHorizontally ||  d->flickingVertically;
}

/*!
    \qmlproperty bool QtQuick1::Flickable::flicking
    \qmlproperty bool QtQuick1::Flickable::flickingHorizontally
    \qmlproperty bool QtQuick1::Flickable::flickingVertically

    These properties describe whether the view is currently moving horizontally,
    vertically or in either direction, due to the user flicking the view.
*/
bool QDeclarative1Flickable::isFlickingHorizontally() const
{
    Q_D(const QDeclarative1Flickable);
    return d->flickingHorizontally;
}

bool QDeclarative1Flickable::isFlickingVertically() const
{
    Q_D(const QDeclarative1Flickable);
    return d->flickingVertically;
}

/*!
    \qmlproperty int QtQuick1::Flickable::pressDelay

    This property holds the time to delay (ms) delivering a press to
    children of the Flickable.  This can be useful where reacting
    to a press before a flicking action has undesirable effects.

    If the flickable is dragged/flicked before the delay times out
    the press event will not be delivered.  If the button is released
    within the timeout, both the press and release will be delivered.

    Note that for nested Flickables with pressDelay set, the pressDelay of
    inner Flickables is overridden by the outermost Flickable.
*/
int QDeclarative1Flickable::pressDelay() const
{
    Q_D(const QDeclarative1Flickable);
    return d->pressDelay;
}

void QDeclarative1Flickable::setPressDelay(int delay)
{
    Q_D(QDeclarative1Flickable);
    if (d->pressDelay == delay)
        return;
    d->pressDelay = delay;
    emit pressDelayChanged();
}


bool QDeclarative1Flickable::isMoving() const
{
    Q_D(const QDeclarative1Flickable);
    return d->movingHorizontally || d->movingVertically;
}

/*!
    \qmlproperty bool QtQuick1::Flickable::moving
    \qmlproperty bool QtQuick1::Flickable::movingHorizontally
    \qmlproperty bool QtQuick1::Flickable::movingVertically

    These properties describe whether the view is currently moving horizontally,
    vertically or in either direction, due to the user either dragging or
    flicking the view.
*/
bool QDeclarative1Flickable::isMovingHorizontally() const
{
    Q_D(const QDeclarative1Flickable);
    return d->movingHorizontally;
}

bool QDeclarative1Flickable::isMovingVertically() const
{
    Q_D(const QDeclarative1Flickable);
    return d->movingVertically;
}

void QDeclarative1Flickable::movementStarting()
{
    Q_D(QDeclarative1Flickable);
    if (d->hMoved && !d->movingHorizontally) {
        d->movingHorizontally = true;
        emit movingChanged();
        emit movingHorizontallyChanged();
        if (!d->movingVertically)
            emit movementStarted();
    }
    else if (d->vMoved && !d->movingVertically) {
        d->movingVertically = true;
        emit movingChanged();
        emit movingVerticallyChanged();
        if (!d->movingHorizontally)
            emit movementStarted();
    }
}

void QDeclarative1Flickable::movementEnding()
{
    Q_D(QDeclarative1Flickable);
    movementXEnding();
    movementYEnding();
    d->hData.smoothVelocity.setValue(0);
    d->vData.smoothVelocity.setValue(0);
}

void QDeclarative1Flickable::movementXEnding()
{
    Q_D(QDeclarative1Flickable);
    if (d->flickingHorizontally) {
        d->flickingHorizontally = false;
        emit flickingChanged();
        emit flickingHorizontallyChanged();
        if (!d->flickingVertically)
           emit flickEnded();
    }
    if (!d->pressed && !d->stealMouse) {
        if (d->movingHorizontally) {
            d->movingHorizontally = false;
            d->hMoved = false;
            emit movingChanged();
            emit movingHorizontallyChanged();
            if (!d->movingVertically)
                emit movementEnded();
        }
    }
    d->hData.fixingUp = false;
}

void QDeclarative1Flickable::movementYEnding()
{
    Q_D(QDeclarative1Flickable);
    if (d->flickingVertically) {
        d->flickingVertically = false;
        emit flickingChanged();
        emit flickingVerticallyChanged();
        if (!d->flickingHorizontally)
           emit flickEnded();
    }
    if (!d->pressed && !d->stealMouse) {
        if (d->movingVertically) {
            d->movingVertically = false;
            d->vMoved = false;
            emit movingChanged();
            emit movingVerticallyChanged();
            if (!d->movingHorizontally)
                emit movementEnded();
        }
    }
    d->vData.fixingUp = false;
}

void QDeclarative1FlickablePrivate::updateVelocity()
{
    Q_Q(QDeclarative1Flickable);
    emit q->horizontalVelocityChanged();
    emit q->verticalVelocityChanged();
}



QT_END_NAMESPACE

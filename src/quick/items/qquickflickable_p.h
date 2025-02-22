// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QQUICKFLICKABLE_P_H
#define QQUICKFLICKABLE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qquickitem.h"
#include <private/qtquickglobal_p.h>

QT_BEGIN_NAMESPACE

class QQuickFlickablePrivate;
class QQuickFlickableVisibleArea;
class Q_QUICK_EXPORT QQuickFlickable : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(qreal contentWidth READ contentWidth WRITE setContentWidth NOTIFY contentWidthChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight WRITE setContentHeight NOTIFY contentHeightChanged)
    Q_PROPERTY(qreal contentX READ contentX WRITE setContentX NOTIFY contentXChanged FINAL)
    Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY contentYChanged FINAL)
    Q_PROPERTY(QQuickItem *contentItem READ contentItem CONSTANT FINAL)

    Q_PROPERTY(qreal topMargin READ topMargin WRITE setTopMargin NOTIFY topMarginChanged FINAL)
    Q_PROPERTY(qreal bottomMargin READ bottomMargin WRITE setBottomMargin NOTIFY bottomMarginChanged FINAL)
    Q_PROPERTY(qreal originY READ originY NOTIFY originYChanged FINAL)

    Q_PROPERTY(qreal leftMargin READ leftMargin WRITE setLeftMargin NOTIFY leftMarginChanged FINAL)
    Q_PROPERTY(qreal rightMargin READ rightMargin WRITE setRightMargin NOTIFY rightMarginChanged FINAL)
    Q_PROPERTY(qreal originX READ originX NOTIFY originXChanged FINAL)

    Q_PROPERTY(qreal horizontalVelocity READ horizontalVelocity NOTIFY horizontalVelocityChanged FINAL)
    Q_PROPERTY(qreal verticalVelocity READ verticalVelocity NOTIFY verticalVelocityChanged FINAL)

    Q_PROPERTY(BoundsBehavior boundsBehavior READ boundsBehavior WRITE setBoundsBehavior NOTIFY boundsBehaviorChanged FINAL)
    Q_PROPERTY(BoundsMovement boundsMovement READ boundsMovement WRITE setBoundsMovement NOTIFY boundsMovementChanged REVISION(2, 10) FINAL)
    Q_PROPERTY(QQuickTransition *rebound READ rebound WRITE setRebound NOTIFY reboundChanged FINAL)
    Q_PROPERTY(qreal maximumFlickVelocity READ maximumFlickVelocity WRITE setMaximumFlickVelocity NOTIFY maximumFlickVelocityChanged FINAL)
    Q_PROPERTY(qreal flickDeceleration READ flickDeceleration WRITE setFlickDeceleration NOTIFY flickDecelerationChanged FINAL)
    Q_PROPERTY(bool moving READ isMoving NOTIFY movingChanged FINAL)
    Q_PROPERTY(bool movingHorizontally READ isMovingHorizontally NOTIFY movingHorizontallyChanged FINAL)
    Q_PROPERTY(bool movingVertically READ isMovingVertically NOTIFY movingVerticallyChanged FINAL)
    Q_PROPERTY(bool flicking READ isFlicking NOTIFY flickingChanged FINAL)
    Q_PROPERTY(bool flickingHorizontally READ isFlickingHorizontally NOTIFY flickingHorizontallyChanged FINAL)
    Q_PROPERTY(bool flickingVertically READ isFlickingVertically NOTIFY flickingVerticallyChanged FINAL)
    Q_PROPERTY(bool dragging READ isDragging NOTIFY draggingChanged FINAL)
    Q_PROPERTY(bool draggingHorizontally READ isDraggingHorizontally NOTIFY draggingHorizontallyChanged FINAL)
    Q_PROPERTY(bool draggingVertically READ isDraggingVertically NOTIFY draggingVerticallyChanged FINAL)
    Q_PROPERTY(FlickableDirection flickableDirection READ flickableDirection WRITE setFlickableDirection NOTIFY flickableDirectionChanged FINAL)

    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY interactiveChanged FINAL)
    Q_PROPERTY(int pressDelay READ pressDelay WRITE setPressDelay NOTIFY pressDelayChanged FINAL)

    Q_PROPERTY(bool atXEnd READ isAtXEnd NOTIFY atXEndChanged FINAL)
    Q_PROPERTY(bool atYEnd READ isAtYEnd NOTIFY atYEndChanged FINAL)
    Q_PROPERTY(bool atXBeginning READ isAtXBeginning NOTIFY atXBeginningChanged FINAL)
    Q_PROPERTY(bool atYBeginning READ isAtYBeginning NOTIFY atYBeginningChanged FINAL)

    Q_PROPERTY(QQuickFlickableVisibleArea *visibleArea READ visibleArea CONSTANT FINAL)

    Q_PROPERTY(bool pixelAligned READ pixelAligned WRITE setPixelAligned NOTIFY pixelAlignedChanged FINAL)
    Q_PROPERTY(bool synchronousDrag READ synchronousDrag WRITE setSynchronousDrag NOTIFY synchronousDragChanged REVISION(2, 12) FINAL)

    Q_PROPERTY(qreal horizontalOvershoot READ horizontalOvershoot NOTIFY horizontalOvershootChanged REVISION(2, 9) FINAL)
    Q_PROPERTY(qreal verticalOvershoot READ verticalOvershoot NOTIFY verticalOvershootChanged REVISION(2, 9) FINAL)

    Q_PROPERTY(QQmlListProperty<QObject> flickableData READ flickableData FINAL)
    Q_PROPERTY(QQmlListProperty<QQuickItem> flickableChildren READ flickableChildren FINAL)
    Q_CLASSINFO("DefaultProperty", "flickableData")
    QML_NAMED_ELEMENT(Flickable)
    QML_ADDED_IN_VERSION(2, 0)

public:
    QQuickFlickable(QQuickItem *parent=nullptr);
    ~QQuickFlickable() override;

    QQmlListProperty<QObject> flickableData();
    QQmlListProperty<QQuickItem> flickableChildren();

    enum BoundsBehaviorFlag {
        StopAtBounds = 0x0,
        DragOverBounds = 0x1,
        OvershootBounds = 0x2,
        DragAndOvershootBounds = DragOverBounds | OvershootBounds
    };
    Q_DECLARE_FLAGS(BoundsBehavior, BoundsBehaviorFlag)
    Q_FLAG(BoundsBehavior)

    BoundsBehavior boundsBehavior() const;
    void setBoundsBehavior(BoundsBehavior);

    enum BoundsMovement {
        // StopAtBounds = 0x0,
        FollowBoundsBehavior = 0x1
    };
    Q_ENUM(BoundsMovement)

    BoundsMovement boundsMovement() const;
    void setBoundsMovement(BoundsMovement movement);

    QQuickTransition *rebound() const;
    void setRebound(QQuickTransition *transition);

    qreal contentWidth() const;
    void setContentWidth(qreal);

    qreal contentHeight() const;
    void setContentHeight(qreal);

    qreal contentX() const;
    virtual void setContentX(qreal pos);

    qreal contentY() const;
    virtual void setContentY(qreal pos);

    qreal topMargin() const;
    void setTopMargin(qreal m);

    qreal bottomMargin() const;
    void setBottomMargin(qreal m);

    qreal leftMargin() const;
    void setLeftMargin(qreal m);

    qreal rightMargin() const;
    void setRightMargin(qreal m);

    virtual qreal originY() const;
    virtual qreal originX() const;

    bool isMoving() const;
    bool isMovingHorizontally() const;
    bool isMovingVertically() const;
    bool isFlicking() const;
    bool isFlickingHorizontally() const;
    bool isFlickingVertically() const;
    bool isDragging() const;
    bool isDraggingHorizontally() const;
    bool isDraggingVertically() const;

    int pressDelay() const;
    void setPressDelay(int delay);

    qreal maximumFlickVelocity() const;
    void setMaximumFlickVelocity(qreal);

    qreal flickDeceleration() const;
    void setFlickDeceleration(qreal);

    bool isInteractive() const;
    void setInteractive(bool);

    qreal horizontalVelocity() const;
    qreal verticalVelocity() const;

    bool isAtXEnd() const;
    bool isAtXBeginning() const;
    bool isAtYEnd() const;
    bool isAtYBeginning() const;

    QQuickItem *contentItem() const;

    enum FlickableDirection { AutoFlickDirection=0x0, HorizontalFlick=0x1, VerticalFlick=0x2, HorizontalAndVerticalFlick=0x3,
                              AutoFlickIfNeeded=0xc };
    Q_ENUM(FlickableDirection)
    FlickableDirection flickableDirection() const;
    void setFlickableDirection(FlickableDirection);

    bool pixelAligned() const;
    void setPixelAligned(bool align);

    bool synchronousDrag() const;
    void setSynchronousDrag(bool v);

    qreal horizontalOvershoot() const;
    qreal verticalOvershoot() const;

    Q_INVOKABLE void resizeContent(qreal w, qreal h, QPointF center);
    Q_INVOKABLE void returnToBounds();
    Q_INVOKABLE void flick(qreal xVelocity, qreal yVelocity);
    Q_INVOKABLE void cancelFlick();

Q_SIGNALS:
    void contentWidthChanged();
    void contentHeightChanged();
    void contentXChanged();
    void contentYChanged();
    void topMarginChanged();
    void bottomMarginChanged();
    void leftMarginChanged();
    void rightMarginChanged();
    void originYChanged();
    void originXChanged();
    void movingChanged();
    void movingHorizontallyChanged();
    void movingVerticallyChanged();
    void flickingChanged();
    void flickingHorizontallyChanged();
    void flickingVerticallyChanged();
    void draggingChanged();
    void draggingHorizontallyChanged();
    void draggingVerticallyChanged();
    void horizontalVelocityChanged();
    void verticalVelocityChanged();
    void isAtBoundaryChanged();
    void flickableDirectionChanged();
    void interactiveChanged();
    void boundsBehaviorChanged();
    Q_REVISION(2, 10) void boundsMovementChanged();
    void reboundChanged();
    void maximumFlickVelocityChanged();
    void flickDecelerationChanged();
    void pressDelayChanged();
    void movementStarted();
    void movementEnded();
    void flickStarted();
    void flickEnded();
    void dragStarted();
    void dragEnded();
    void pixelAlignedChanged();
    Q_REVISION(2, 12) void synchronousDragChanged();
    Q_REVISION(2, 9) void horizontalOvershootChanged();
    Q_REVISION(2, 9) void verticalOvershootChanged();

    // The next four signals should be marked as Q_REVISION(2, 12). See QTBUG-71243
    void atXEndChanged();
    void atYEndChanged();
    void atXBeginningChanged();
    void atYBeginningChanged();

protected:
    bool childMouseEventFilter(QQuickItem *, QEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void touchEvent(QTouchEvent *event) override;
#if QT_CONFIG(wheelevent)
    void wheelEvent(QWheelEvent *event) override;
#endif
    void timerEvent(QTimerEvent *event) override;

    QQuickFlickableVisibleArea *visibleArea();

protected Q_SLOTS:
    void movementStarting();
    void movementEnding();
    void movementEnding(bool hMovementEnding, bool vMovementEnding);
    void velocityTimelineCompleted();
    void timelineCompleted();

protected:
    virtual qreal minXExtent() const;
    virtual qreal minYExtent() const;
    virtual qreal maxXExtent() const;
    virtual qreal maxYExtent() const;
    qreal vWidth() const;
    qreal vHeight() const;
    void componentComplete() override;
    virtual void viewportMoved(Qt::Orientations orient);
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mouseUngrabEvent() override;
    bool filterPointerEvent(QQuickItem *receiver, QPointerEvent *event);

    bool xflick() const;
    bool yflick() const;

protected:
    QQuickFlickable(QQuickFlickablePrivate &dd, QQuickItem *parent);

private:
    Q_DISABLE_COPY(QQuickFlickable)
    Q_DECLARE_PRIVATE(QQuickFlickable)
    friend class QQuickFlickableVisibleArea;
    friend class QQuickFlickableReboundTransition;
};

QT_END_NAMESPACE

#endif // QQUICKFLICKABLE_P_H

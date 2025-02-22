// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QQUICKPINCHAREA_H
#define QQUICKPINCHAREA_H

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

#include <private/qtquickglobal_p.h>

#include "qquickitem.h"

QT_BEGIN_NAMESPACE

class Q_QUICK_EXPORT QQuickPinch : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQuickItem *target READ target WRITE setTarget RESET resetTarget NOTIFY targetChanged FINAL)
    Q_PROPERTY(qreal minimumScale READ minimumScale WRITE setMinimumScale NOTIFY minimumScaleChanged FINAL)
    Q_PROPERTY(qreal maximumScale READ maximumScale WRITE setMaximumScale NOTIFY maximumScaleChanged FINAL)
    Q_PROPERTY(qreal minimumRotation READ minimumRotation WRITE setMinimumRotation NOTIFY minimumRotationChanged FINAL)
    Q_PROPERTY(qreal maximumRotation READ maximumRotation WRITE setMaximumRotation NOTIFY maximumRotationChanged FINAL)
    Q_PROPERTY(Axis dragAxis READ axis WRITE setAxis NOTIFY dragAxisChanged FINAL)
    Q_PROPERTY(qreal minimumX READ xmin WRITE setXmin NOTIFY minimumXChanged FINAL)
    Q_PROPERTY(qreal maximumX READ xmax WRITE setXmax NOTIFY maximumXChanged FINAL)
    Q_PROPERTY(qreal minimumY READ ymin WRITE setYmin NOTIFY minimumYChanged FINAL)
    Q_PROPERTY(qreal maximumY READ ymax WRITE setYmax NOTIFY maximumYChanged FINAL)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)
    QML_NAMED_ELEMENT(Pinch)
    QML_ADDED_IN_VERSION(2, 0)

public:
    QQuickPinch();

    QQuickItem *target() const { return m_target; }
    void setTarget(QQuickItem *target) {
        if (target == m_target)
            return;
        m_target = target;
        Q_EMIT targetChanged();
    }
    void resetTarget() {
        if (!m_target)
            return;
        m_target = nullptr;
        Q_EMIT targetChanged();
    }

    qreal minimumScale() const { return m_minScale; }
    void setMinimumScale(qreal s) {
        if (s == m_minScale)
            return;
        m_minScale = s;
        Q_EMIT minimumScaleChanged();
    }
    qreal maximumScale() const { return m_maxScale; }
    void setMaximumScale(qreal s) {
        if (s == m_maxScale)
            return;
        m_maxScale = s;
        Q_EMIT maximumScaleChanged();
    }

    qreal minimumRotation() const { return m_minRotation; }
    void setMinimumRotation(qreal r) {
        if (r == m_minRotation)
            return;
        m_minRotation = r;
        Q_EMIT minimumRotationChanged();
    }
    qreal maximumRotation() const { return m_maxRotation; }
    void setMaximumRotation(qreal r) {
        if (r == m_maxRotation)
            return;
        m_maxRotation = r;
        Q_EMIT maximumRotationChanged();
    }

    enum Axis { NoDrag=0x00, XAxis=0x01, YAxis=0x02, XAndYAxis=0x03, XandYAxis=XAndYAxis };
    Q_ENUM(Axis)
    Axis axis() const { return m_axis; }
    void setAxis(Axis a) {
        if (a == m_axis)
            return;
        m_axis = a;
        Q_EMIT dragAxisChanged();
    }

    qreal xmin() const { return m_xmin; }
    void setXmin(qreal x) {
        if (x == m_xmin)
            return;
        m_xmin = x;
        Q_EMIT minimumXChanged();
    }
    qreal xmax() const { return m_xmax; }
    void setXmax(qreal x) {
        if (x == m_xmax)
            return;
        m_xmax = x;
        Q_EMIT maximumXChanged();
    }
    qreal ymin() const { return m_ymin; }
    void setYmin(qreal y) {
        if (y == m_ymin)
            return;
        m_ymin = y;
        Q_EMIT minimumYChanged();
    }
    qreal ymax() const { return m_ymax; }
    void setYmax(qreal y) {
        if (y == m_ymax)
            return;
        m_ymax = y;
        Q_EMIT maximumYChanged();
    }

    bool active() const { return m_active; }
    void setActive(bool a) {
        if (a == m_active)
            return;
        m_active = a;
        Q_EMIT activeChanged();
    }

Q_SIGNALS:
    void targetChanged();
    void minimumScaleChanged();
    void maximumScaleChanged();
    void minimumRotationChanged();
    void maximumRotationChanged();
    void dragAxisChanged();
    void minimumXChanged();
    void maximumXChanged();
    void minimumYChanged();
    void maximumYChanged();
    void activeChanged();

private:
    QQuickItem *m_target;
    qreal m_minScale;
    qreal m_maxScale;
    qreal m_minRotation;
    qreal m_maxRotation;
    Axis m_axis;
    qreal m_xmin;
    qreal m_xmax;
    qreal m_ymin;
    qreal m_ymax;
    bool m_active;
};

class Q_QUICK_EXPORT QQuickPinchEvent : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QPointF center READ center FINAL)
    Q_PROPERTY(QPointF startCenter READ startCenter FINAL)
    Q_PROPERTY(QPointF previousCenter READ previousCenter FINAL)
    Q_PROPERTY(qreal scale READ scale FINAL)
    Q_PROPERTY(qreal previousScale READ previousScale FINAL)
    Q_PROPERTY(qreal angle READ angle FINAL)
    Q_PROPERTY(qreal previousAngle READ previousAngle FINAL)
    Q_PROPERTY(qreal rotation READ rotation FINAL)
    Q_PROPERTY(QPointF point1 READ point1 FINAL)
    Q_PROPERTY(QPointF startPoint1 READ startPoint1 FINAL)
    Q_PROPERTY(QPointF point2 READ point2 FINAL)
    Q_PROPERTY(QPointF startPoint2 READ startPoint2 FINAL)
    Q_PROPERTY(int pointCount READ pointCount FINAL)
    Q_PROPERTY(bool accepted READ accepted WRITE setAccepted FINAL)
    QML_ANONYMOUS
    QML_ADDED_IN_VERSION(2, 0)

public:
    QQuickPinchEvent(QPointF c, qreal s, qreal a, qreal r)
        : QObject(), m_center(c), m_scale(s), m_angle(a), m_rotation(r)
        , m_pointCount(0), m_accepted(true) {}

    QPointF center() const { return m_center; }
    QPointF startCenter() const { return m_startCenter; }
    void setStartCenter(QPointF c) { m_startCenter = c; }
    QPointF previousCenter() const { return m_lastCenter; }
    void setPreviousCenter(QPointF c) { m_lastCenter = c; }
    qreal scale() const { return m_scale; }
    qreal previousScale() const { return m_lastScale; }
    void setPreviousScale(qreal s) { m_lastScale = s; }
    qreal angle() const { return m_angle; }
    qreal previousAngle() const { return m_lastAngle; }
    void setPreviousAngle(qreal a) { m_lastAngle = a; }
    qreal rotation() const { return m_rotation; }
    QPointF point1() const { return m_point1; }
    void setPoint1(QPointF p) { m_point1 = p; }
    QPointF startPoint1() const { return m_startPoint1; }
    void setStartPoint1(QPointF p) { m_startPoint1 = p; }
    QPointF point2() const { return m_point2; }
    void setPoint2(QPointF p) { m_point2 = p; }
    QPointF startPoint2() const { return m_startPoint2; }
    void setStartPoint2(QPointF p) { m_startPoint2 = p; }
    int pointCount() const { return m_pointCount; }
    void setPointCount(int count) { m_pointCount = count; }

    bool accepted() const { return m_accepted; }
    void setAccepted(bool a) { m_accepted = a; }

private:
    QPointF m_center;
    QPointF m_startCenter;
    QPointF m_lastCenter;
    qreal m_scale;
    qreal m_lastScale;
    qreal m_angle;
    qreal m_lastAngle;
    qreal m_rotation;
    QPointF m_point1;
    QPointF m_point2;
    QPointF m_startPoint1;
    QPointF m_startPoint2;
    int m_pointCount;
    bool m_accepted;
};


class QQuickPinchAreaPrivate;
class Q_QUICK_EXPORT QQuickPinchArea : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(QQuickPinch *pinch READ pinch CONSTANT FINAL)
    QML_NAMED_ELEMENT(PinchArea)
    QML_ADDED_IN_VERSION(2, 0)

public:
    QQuickPinchArea(QQuickItem *parent=nullptr);
    ~QQuickPinchArea();

    bool isEnabled() const;
    void setEnabled(bool);

    QQuickPinch *pinch();

Q_SIGNALS:
    void enabledChanged();
    void pinchStarted(QQuickPinchEvent *pinch);
    void pinchUpdated(QQuickPinchEvent *pinch);
    void pinchFinished(QQuickPinchEvent *pinch);
    Q_REVISION(2, 5) void smartZoom(QQuickPinchEvent *pinch);

protected:
    bool childMouseEventFilter(QQuickItem *i, QEvent *e) override;
    void touchEvent(QTouchEvent *event) override;

    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    bool event(QEvent *) override;

private:
    void clearPinch(QTouchEvent *event);
    void cancelPinch(QTouchEvent *event);
    void updatePinch(QTouchEvent *event, bool filtering);
    void updatePinchTarget();

private:
    Q_DISABLE_COPY(QQuickPinchArea)
    Q_DECLARE_PRIVATE(QQuickPinchArea)
};

QT_END_NAMESPACE

#endif // QQUICKPINCHAREA_H


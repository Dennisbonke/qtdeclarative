// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qquickmaterialplaceholdertext_p.h"

#include <QtCore/qpropertyanimation.h>
#include <QtCore/qparallelanimationgroup.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpainterpath.h>
#include <QtQml/qqmlinfo.h>
#include <QtQuickTemplates2/private/qquicktheme_p.h>

QT_BEGIN_NAMESPACE

static const qreal floatingScale = 0.8;
Q_GLOBAL_STATIC(QEasingCurve, animationEasingCurve, QEasingCurve::OutSine);

/*
    This class makes it easier to animate the various placeholder text changes
    for each type of text container (filled, outlined).

    By doing animations in C++, we avoid having a bunch of states, transitions,
    and animations (which are all QObjects) declared in QML, even if that text
    control never gets focus and hence never needs them.
*/

QQuickMaterialPlaceholderText::QQuickMaterialPlaceholderText(QQuickItem *parent)
    : QQuickPlaceholderText(parent)
{
    // Ensure that scaling happens on the left side, at the vertical center.
    setTransformOrigin(QQuickItem::Left);
}

bool QQuickMaterialPlaceholderText::isFilled() const
{
    return m_filled;
}

void QQuickMaterialPlaceholderText::setFilled(bool filled)
{
    if (filled == m_filled)
        return;

    m_filled = filled;
    update();
    void filledChanged();
}

bool QQuickMaterialPlaceholderText::controlHasActiveFocus() const
{
    return m_controlHasActiveFocus;
}

void QQuickMaterialPlaceholderText::setControlHasActiveFocus(bool controlHasActiveFocus)
{
    if (m_controlHasActiveFocus == controlHasActiveFocus)
        return;

    m_controlHasActiveFocus = controlHasActiveFocus;
    if (m_controlHasActiveFocus)
        controlGotActiveFocus();
    else
        controlLostActiveFocus();
    emit controlHasActiveFocusChanged();
}

bool QQuickMaterialPlaceholderText::controlHasText() const
{
    return m_controlHasText;
}

void QQuickMaterialPlaceholderText::setControlHasText(bool controlHasText)
{
    if (m_controlHasText == controlHasText)
        return;

    m_controlHasText = controlHasText;
    maybeSetFocusAnimationProgress();
    emit controlHasTextChanged();
}

/*
    Placeholder text of outlined text fields should float when:
    - There is placeholder text, and
      - The control has active focus, or
      - The control has text
*/
bool QQuickMaterialPlaceholderText::shouldFloat() const
{
    const bool controlHasActiveFocusOrText = m_controlHasActiveFocus || m_controlHasText;
    return m_filled
        ? controlHasActiveFocusOrText
        : !text().isEmpty() && controlHasActiveFocusOrText;
}

bool QQuickMaterialPlaceholderText::shouldAnimate() const
{
    return m_filled
        ? !m_controlHasText
        : !m_controlHasText && !text().isEmpty();
}

qreal QQuickMaterialPlaceholderText::normalTargetY() const
{
    // When the placeholder text shouldn't float, it should sit in the middle of the TextField.
    // We could just use the control's height minus our height instead of the members, but
    // that doesn't work for TextArea, which can be multiple lines in height and hence taller.
    // In that case, we want the placeholder text to sit in the middle of its default-height (one-line).
    return (m_controlImplicitBackgroundHeight - m_largestHeight) / 2.0;
}

qreal QQuickMaterialPlaceholderText::floatingTargetY() const
{
    // For filled text fields, the placeholder text sits just above
    // the text when floating.
    if (m_filled)
        return m_verticalPadding;

    // Outlined text fields have the placeaholder vertically centered
    // along the outline at the top.
    return -m_largestHeight / 2;
}

/*!
    \internal

    The height of the text at its largest size that we set.
*/
int QQuickMaterialPlaceholderText::largestHeight() const
{
    return m_largestHeight;
}

qreal QQuickMaterialPlaceholderText::controlImplicitBackgroundHeight() const
{
    return m_controlImplicitBackgroundHeight;
}

void QQuickMaterialPlaceholderText::setControlImplicitBackgroundHeight(qreal controlImplicitBackgroundHeight)
{
    if (qFuzzyCompare(m_controlImplicitBackgroundHeight, controlImplicitBackgroundHeight))
        return;

    m_controlImplicitBackgroundHeight = controlImplicitBackgroundHeight;
    setY(shouldFloat() ? floatingTargetY() : normalTargetY());
    emit controlImplicitBackgroundHeightChanged();
}

qreal QQuickMaterialPlaceholderText::verticalPadding() const
{
    return m_verticalPadding;
}

void QQuickMaterialPlaceholderText::setVerticalPadding(qreal verticalPadding)
{
    if (qFuzzyCompare(m_verticalPadding, verticalPadding))
        return;

    m_verticalPadding = verticalPadding;
    emit verticalPaddingChanged();
}

void QQuickMaterialPlaceholderText::controlGotActiveFocus()
{
    if (m_focusOutAnimation)
        m_focusOutAnimation->stop();

    Q_ASSERT(!m_focusInAnimation);
    if (shouldAnimate()) {
        m_focusInAnimation = new QParallelAnimationGroup(this);

        QPropertyAnimation *yAnimation = new QPropertyAnimation(this, "y", this);
        yAnimation->setDuration(300);
        yAnimation->setStartValue(y());
        yAnimation->setEndValue(floatingTargetY());
        yAnimation->setEasingCurve(*animationEasingCurve);
        m_focusInAnimation->addAnimation(yAnimation);

        auto *scaleAnimation = new QPropertyAnimation(this, "scale", this);
        scaleAnimation->setDuration(300);
        scaleAnimation->setStartValue(1);
        scaleAnimation->setEndValue(floatingScale);
        yAnimation->setEasingCurve(*animationEasingCurve);
        m_focusInAnimation->addAnimation(scaleAnimation);

        m_focusInAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        const int newY = shouldFloat() ? floatingTargetY() : normalTargetY();
        setY(newY);
    }
}

void QQuickMaterialPlaceholderText::controlLostActiveFocus()
{
    Q_ASSERT(!m_focusOutAnimation);
    if (shouldAnimate()) {
        m_focusOutAnimation = new QParallelAnimationGroup(this);

        auto *yAnimation = new QPropertyAnimation(this, "y", this);
        yAnimation->setDuration(300);
        yAnimation->setStartValue(y());
        yAnimation->setEndValue(normalTargetY());
        yAnimation->setEasingCurve(*animationEasingCurve);
        m_focusOutAnimation->addAnimation(yAnimation);

        auto *scaleAnimation = new QPropertyAnimation(this, "scale", this);
        scaleAnimation->setDuration(300);
        scaleAnimation->setStartValue(floatingScale);
        scaleAnimation->setEndValue(1);
        yAnimation->setEasingCurve(*animationEasingCurve);
        m_focusOutAnimation->addAnimation(scaleAnimation);

        m_focusOutAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        const int newY = shouldFloat() ? floatingTargetY() : normalTargetY();
        setY(newY);
    }
}

void QQuickMaterialPlaceholderText::maybeSetFocusAnimationProgress()
{
    const bool shouldWeFloat = shouldFloat();
    setY(shouldWeFloat ? floatingTargetY() : normalTargetY());
    setScale(shouldWeFloat ? floatingScale : 1.0);
}

void QQuickMaterialPlaceholderText::componentComplete()
{
    QQuickPlaceholderText::componentComplete();

    if (!parentItem())
        qmlWarning(this) << "Expected parent item by component completion!";

    m_largestHeight = implicitHeight();
    if (m_largestHeight > 0) {
        emit largestHeightChanged();
    } else {
        qmlWarning(this) << "Expected implicitHeight of placeholder text" << text()
            << "to be greater than 0 by component completion!";
    }

    maybeSetFocusAnimationProgress();
}

QT_END_NAMESPACE

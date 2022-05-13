// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qquickplaceholdertext_p.h"

#include <QtQuick/private/qquicktext_p_p.h>
#include <QtQuick/private/qquicktextinput_p_p.h>
#include <QtQuick/private/qquicktextedit_p_p.h>

QT_BEGIN_NAMESPACE

QQuickPlaceholderText::QQuickPlaceholderText(QQuickItem *parent) : QQuickText(parent)
{
}

void QQuickPlaceholderText::componentComplete()
{
    QQuickText::componentComplete();
    connect(parentItem(), SIGNAL(effectiveHorizontalAlignmentChanged()), this, SLOT(updateAlignment()));
    updateAlignment();
}

void QQuickPlaceholderText::updateAlignment()
{
    if (QQuickTextInput *input = qobject_cast<QQuickTextInput *>(parentItem())) {
        if (QQuickTextInputPrivate::get(input)->hAlignImplicit)
            resetHAlign();
        else
            setHAlign(static_cast<HAlignment>(input->hAlign()));
    } else if (QQuickTextEdit *edit = qobject_cast<QQuickTextEdit *>(parentItem())) {
        if (QQuickTextEditPrivate::get(edit)->hAlignImplicit)
            resetHAlign();
        else
            setHAlign(static_cast<HAlignment>(edit->hAlign()));
    } else {
        resetHAlign();
    }
}

QT_END_NAMESPACE

#include "moc_qquickplaceholdertext_p.cpp"

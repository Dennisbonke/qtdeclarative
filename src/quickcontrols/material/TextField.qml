// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.impl
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl

T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    leftPadding: Material.textFieldHorizontalPadding
    rightPadding: Material.textFieldHorizontalPadding
    // Need to account for the placeholder text when it's sitting on top.
    topPadding: Material.containerStyle === Material.Filled
        ? placeholderText.length > 0 && (activeFocus || length > 0)
            ? Material.textFieldVerticalPadding + placeholder.largestHeight
            : Material.textFieldVerticalPadding
        : Material.textFieldVerticalPadding
    bottomPadding: Material.textFieldVerticalPadding

    color: enabled ? Material.foreground : Material.hintTextColor
    selectionColor: Material.accentColor
    selectedTextColor: Material.primaryHighlightedTextColor
    placeholderTextColor: Material.hintTextColor
    verticalAlignment: TextInput.AlignVCenter

    Material.containerStyle: Material.Outlined

    cursorDelegate: CursorDelegate { }

    FloatingPlaceholderText {
        id: placeholder
        x: control.leftPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        elide: Text.ElideRight
        renderType: control.renderType

        filled: control.Material.containerStyle === Material.Filled
        verticalPadding: control.Material.textFieldVerticalPadding
        controlHasActiveFocus: control.activeFocus
        controlHasText: control.length > 0
        controlImplicitBackgroundHeight: control.implicitBackgroundHeight
    }

    background: MaterialTextContainer {
        implicitWidth: 120
        implicitHeight: control.Material.textFieldHeight

        filled: control.Material.containerStyle === Material.Filled
        fillColor: control.Material.textFieldFilledContainerColor
        outlineColor: (enabled && control.hovered) ? control.Material.primaryTextColor : control.Material.hintTextColor
        focusedOutlineColor: control.Material.accentColor
        // When the control's size is set larger than its implicit size, use whatever size is smaller
        // so that the gap isn't too big.
        placeholderTextWidth: Math.min(placeholder.width, placeholder.implicitWidth) * placeholder.scale
        controlHasActiveFocus: control.activeFocus
        controlHasText: control.length > 0
        placeholderHasText: placeholder.text.length > 0
        horizontalPadding: control.Material.textFieldHorizontalPadding
    }
}

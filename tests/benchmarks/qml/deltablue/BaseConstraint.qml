// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

pragma Strict
import QtQml

QtObject {
    required property int strength
    property int satisfaction: Satisfaction.NONE
    property bool isInput: false
}

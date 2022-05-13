// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 400
    height: 400

    property alias drawer: drawer
    property alias popup: popup

    Drawer {
        id: drawer
        width: 200
        height: parent.height
    }

    Popup {
        id: popup
        x: 200
        width: 200
        height: parent.height
    }
}

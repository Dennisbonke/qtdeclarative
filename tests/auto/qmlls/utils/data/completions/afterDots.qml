// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

import QtQuick

QtObject {
    id: root
    property int good
    property var i: Item {
       property int bad
       property int myP: root.
       Item { }
       property int myP2: root.;
       bad: 43
       function f() {
           root.;
           for (;;) {}
       }
   }
}

// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
//![0]
// exampleFive.qml
import QtQuick 2.0
import Qt.example 1.0
import "exampleFour.js" as ExampleFourJs // use factory from example four

QtObject {
    property var avatarOne: ExampleFourJs.importAvatar()
    property var avatarTwo: avatarOne

    Component.onCompleted: {
        ExampleFourJs.releaseAvatar(avatarTwo);
    }
}
//![0]

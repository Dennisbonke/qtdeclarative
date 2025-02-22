# Copyright (C) 2023 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(chapter3 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
    set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/quickcontrols/chattutorial/chapter3")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

qt_add_executable(chattutorial-chapter3 WIN32 MACOSX_BUNDLE
    main.cpp
)

target_link_libraries(chattutorial-chapter3 PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

qt_policy(SET QTP0001 NEW)
qt_add_qml_module(chattutorial-chapter3
    URI chattutorial
    QML_FILES
        "ContactPage.qml"
        "ConversationPage.qml"
        "Main.qml"
    RESOURCES
        "images/Albert_Einstein.png"
        "images/Albert_Einstein@2x.png"
        "images/Albert_Einstein@3x.png"
        "images/Albert_Einstein@4x.png"
        "images/Ernest_Hemingway.png"
        "images/Ernest_Hemingway@2x.png"
        "images/Ernest_Hemingway@3x.png"
        "images/Ernest_Hemingway@4x.png"
        "images/Hans_Gude.png"
        "images/Hans_Gude@2x.png"
        "images/Hans_Gude@3x.png"
        "images/Hans_Gude@4x.png"
)

qt6_add_resources(chattutorial-chapter3 "conf"
    PREFIX
        "/"
    FILES
        "qtquickcontrols2.conf"
)

install(TARGETS chattutorial-chapter3
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)

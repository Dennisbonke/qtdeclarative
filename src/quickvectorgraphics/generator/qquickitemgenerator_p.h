// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QQUICKITEMGENERATOR_P_H
#define QQUICKITEMGENERATOR_P_H

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

#include "qquickgenerator_p.h"
#include <QStack>

QT_BEGIN_NAMESPACE

class Q_QUICKVECTORGRAPHICSGENERATOR_EXPORT QQuickItemGenerator : public QQuickGenerator
{
public:
    QQuickItemGenerator(const QString fileName, QQuickVectorGraphics::GeneratorFlags flags, QQuickItem *parentItem);
    ~QQuickItemGenerator();

protected:
    void generateNodeBase(const NodeInfo &info) override;
    bool generateDefsNode(const NodeInfo &info) override;
    void generateImageNode(const ImageNodeInfo &info) override;
    void generatePath(const PathNodeInfo &info) override;
    void generateNode(const NodeInfo &info) override;
    void generateTextNode(const TextNodeInfo &info) override;
    void generateStructureNode(const StructureNodeInfo &info) override;
    void generateRootNode(const StructureNodeInfo &info) override;
    void outputShapePath(const PathNodeInfo &info, const QPainterPath *path, const QQuadPath *quadPath, QQuickVectorGraphics::PathSelector pathSelector, const QRectF &boundingRect) override;

private:
    void generateGradient(const QGradient *grad, QQuickShapePath *shapePath, const QRectF &boundingRect);
    QQuickItem *currentItem();
    void addCurrentItem(QQuickItem *item, const NodeInfo &info);

    bool m_inShapeItem = false;
    QQuickShape *m_parentShapeItem = nullptr;

    QStack<QQuickItem *> m_items;

    QQuickItem *m_parentItem = nullptr;
};

QT_END_NAMESPACE

#endif // QQUICKITEMGENERATOR_P_H

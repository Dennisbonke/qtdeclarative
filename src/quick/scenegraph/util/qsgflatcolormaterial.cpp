/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgflatcolormaterial.h"
#include <private/qsgmaterialshader_p.h>

QT_BEGIN_NAMESPACE

class FlatColorMaterialRhiShader : public QSGMaterialShader
{
public:
    FlatColorMaterialRhiShader();

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;

    static QSGMaterialType type;
};

QSGMaterialType FlatColorMaterialRhiShader::type;

FlatColorMaterialRhiShader::FlatColorMaterialRhiShader()
{
    setShaderFileName(VertexStage, QStringLiteral(":/qt-project.org/scenegraph/shaders_ng/flatcolor.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/qt-project.org/scenegraph/shaders_ng/flatcolor.frag.qsb"));
}

bool FlatColorMaterialRhiShader::updateUniformData(RenderState &state,
                                                   QSGMaterial *newMaterial,
                                                   QSGMaterial *oldMaterial)
{
    Q_ASSERT(!oldMaterial || newMaterial->type() == oldMaterial->type());
    QSGFlatColorMaterial *oldMat = static_cast<QSGFlatColorMaterial *>(oldMaterial);
    QSGFlatColorMaterial *mat = static_cast<QSGFlatColorMaterial *>(newMaterial);
    bool changed = false;
    QByteArray *buf = state.uniformData();

    if (state.isMatrixDirty()) {
        const QMatrix4x4 m = state.combinedMatrix();
        memcpy(buf->data(), m.constData(), 64);
        changed = true;
    }

    const QColor &c = mat->color();
    if (!oldMat || c != oldMat->color() || state.isOpacityDirty()) {
        const float opacity = state.opacity() * c.alphaF();
        QVector4D v(c.redF() * opacity,
                    c.greenF() * opacity,
                    c.blueF() * opacity,
                    opacity);
        Q_ASSERT(sizeof(v) == 16);
        memcpy(buf->data() + 64, &v, 16);
        changed = true;
    }

    return changed;
}


/*!
    \class QSGFlatColorMaterial

    \brief The QSGFlatColorMaterial class provides a convenient way of rendering
    solid colored geometry in the scene graph.

    \inmodule QtQuick
    \ingroup qtquick-scenegraph-materials

    \warning This utility class is only functional when running with the default
    backend of the Qt Quick scenegraph.

    The flat color material will fill every pixel in a geometry using
    a solid color. The color can contain transparency.

    The geometry to be rendered with a flat color material requires
    vertices in attribute location 0 in the QSGGeometry object to render
    correctly. The QSGGeometry::defaultAttributes_Point2D() returns an attribute
    set compatible with this material.

    The flat color material respects both current opacity and current matrix
    when updating its rendering state.
 */


/*!
    Constructs a new flat color material.

    The default color is white.
 */

QSGFlatColorMaterial::QSGFlatColorMaterial() : m_color(QColor(255, 255, 255))
{
}

/*!
    \fn const QColor &QSGFlatColorMaterial::color() const

    Returns this flat color material's color.

    The default color is white.
 */



/*!
    Sets this flat color material's color to \a color.
 */

void QSGFlatColorMaterial::setColor(const QColor &color)
{
    m_color = color;
    setFlag(Blending, m_color.alpha() != 0xff);
}



/*!
    \internal
 */

QSGMaterialType *QSGFlatColorMaterial::type() const
{
    return &FlatColorMaterialRhiShader::type;
}



/*!
    \internal
 */

QSGMaterialShader *QSGFlatColorMaterial::createShader() const
{
    return new FlatColorMaterialRhiShader;
}


/*!
    \internal
 */

int QSGFlatColorMaterial::compare(const QSGMaterial *other) const
{
    const QSGFlatColorMaterial *flat = static_cast<const QSGFlatColorMaterial *>(other);
    return m_color.rgba() - flat->color().rgba();

}

QT_END_NAMESPACE

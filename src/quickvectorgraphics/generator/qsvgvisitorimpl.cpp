// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "qsvgvisitorimpl_p.h"
#include "qquickgenerator_p.h"
#include "qquicknodeinfo_p.h"

#include <private/qsvgvisitor_p.h>

#include <QString>
#include <QPainter>
#include <QMatrix4x4>
#include <QQuickItem>

#include <private/qquickshape_p.h>
#include <private/qquicktext_p.h>
#include <private/qquicktranslate_p.h>
#include <private/qquickitem_p.h>

#include <private/qquickimagebase_p_p.h>
#include <private/qquickimage_p.h>
#include <private/qsgcurveprocessor_p.h>

#include <private/qquadpath_p.h>

#include "utils_p.h"
#include<QtCore/qloggingcategory.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcQuickVectorGraphics)

static inline bool isPathContainer(const QSvgStructureNode *node)
{
    bool foundPath = false;
    for (const auto *child : node->renderers()) {
        switch (child->type()) {
            // nodes that shouldn't go inside Shape{}
        case QSvgNode::Switch:
        case QSvgNode::Doc:
        case QSvgNode::Group:
        case QSvgNode::Animation:
        case QSvgNode::Use:
        case QSvgNode::Video:
            //qCDebug(lcQuickVectorGraphics) << "NOT path container because" << node->typeName() ;
            return false;

            // nodes that could go inside Shape{}
        case QSvgNode::Defs:
        case QSvgNode::Image:
        case QSvgNode::Textarea:
        case QSvgNode::Text:
        case QSvgNode::Tspan:
            break;

            // nodes that are done as pure ShapePath{}
        case QSvgNode::Rect:
        case QSvgNode::Circle:
        case QSvgNode::Ellipse:
        case QSvgNode::Line:
        case QSvgNode::Path:
        case QSvgNode::Polygon:
        case QSvgNode::Polyline:
            if (!child->style().transform.isDefault()) {
                //qCDebug(lcQuickVectorGraphics) << "NOT path container because local transform";
                return false;
            }
            foundPath = true;
            break;
        default:
            qCDebug(lcQuickVectorGraphics) << "Unhandled type in switch" << child->type();
            break;
        }
    }
    //qCDebug(lcQuickVectorGraphics) << "Container" << node->nodeId() << node->typeName()  << "is" << foundPath;
    return foundPath;
}

class QSvgStyleResolver
{
public:
    QSvgStyleResolver()
    {
        m_dummyImage = QImage(1, 1, QImage::Format_RGB32);
        m_dummyPainter.begin(&m_dummyImage);
        QPen noPen(Qt::NoPen);
        noPen.setBrush(Qt::NoBrush);
        m_dummyPainter.setPen(noPen);
        m_dummyPainter.setBrush(Qt::black);
    }

    ~QSvgStyleResolver()
    {
        m_dummyPainter.end();
    }

    QPainter& painter() { return m_dummyPainter; }
    QSvgExtraStates& states() { return m_svgState; }

    QString currentFillColor() const
    {
        if (m_dummyPainter.brush().style() != Qt::NoBrush) {
            QColor c(m_dummyPainter.brush().color());
            c.setAlphaF(m_svgState.fillOpacity);
            //qCDebug(lcQuickVectorGraphics) << "FILL" << c << m_svgState.fillOpacity << c.name();
            return c.name(QColor::HexArgb);
        } else {
            return QStringLiteral("transparent");
        }
    }

    const QGradient *currentFillGradient() const
    {
        if (m_dummyPainter.brush().style() == Qt::LinearGradientPattern || m_dummyPainter.brush().style() == Qt::RadialGradientPattern || m_dummyPainter.brush().style() == Qt::ConicalGradientPattern )
            return m_dummyPainter.brush().gradient();
        return nullptr;
    }

    QString currentStrokeColor() const
    {
        if (m_dummyPainter.pen().style() != Qt::NoPen)
            return m_dummyPainter.pen().color().name();
        else if (m_dummyPainter.pen().brush().style() == Qt::SolidPattern)
            return m_dummyPainter.pen().brush().color().name();
        return {};
    }

    float currentStrokeWidth() const
    {
        float penWidth = m_dummyPainter.pen().widthF();
        return penWidth ? penWidth : 1;
    }

protected:
    QPainter m_dummyPainter;
    QImage m_dummyImage;
    QSvgExtraStates m_svgState;

};

Q_GLOBAL_STATIC(QSvgStyleResolver, styleResolver)

QSvgVisitorImpl::QSvgVisitorImpl(const QString svgFileName, QQuickGenerator *generator)
    : m_svgFileName(svgFileName)
    , m_generator(generator)
{

}

void QSvgVisitorImpl::traverse()
{
    if (!m_generator) {
        qCDebug(lcQuickVectorGraphics) << "No valid QQuickGenerator is set. Genration will stop";
        return;
    }

    auto *doc = QSvgTinyDocument::load(m_svgFileName);
    if (!doc) {
        qCDebug(lcQuickVectorGraphics) << "Not a valid Svg File : " << m_svgFileName;
        return;
    }

    QSvgVisitor::traverse(doc);
}

void QSvgVisitorImpl::visitNode(const QSvgNode *node)
{
    handleBaseNodeSetup(node);

    ImageNodeInfo info;
    fillCommonNodeInfo(node, info);

    m_generator->generateNode(info);

    handleBaseNodeEnd(node);
}

void QSvgVisitorImpl::visitImageNode(const QSvgImage *node)
{
    // TODO: this requires proper asset management.
    handleBaseNodeSetup(node);

    ImageNodeInfo info;
    fillCommonNodeInfo(node, info);
    info.image = node->image();
    info.rect = node->rect();

    m_generator->generateImageNode(info);

    handleBaseNodeEnd(node);
}

void QSvgVisitorImpl::visitRectNode(const QSvgRect *node)
{
    QRectF rect = node->rect();
    QPointF rads = node->radius();
    // This is using Qt::RelativeSize semantics: percentage of half rect size
    qreal x1 = rect.left();
    qreal x2 = rect.right();
    qreal y1 = rect.top();
    qreal y2 = rect.bottom();

    qreal rx =  rads.x() * rect.width() / 200;
    qreal ry = rads.y() * rect.height() / 200;
    QPainterPath p;

    p.moveTo(x1 + rx, y1);
    p.lineTo(x2 - rx, y1);
    // qCDebug(lcQuickVectorGraphics) << "Line1" << x2 - rx << y1;
    p.arcTo(x2 - rx * 2, y1, rx * 2, ry * 2, 90, -90); // ARC to x2, y1 + ry
    // qCDebug(lcQuickVectorGraphics) << "p1" << p;

    p.lineTo(x2, y2 - ry);
    p.arcTo(x2 - rx * 2, y2 - ry * 2, rx * 2, ry * 2, 0, -90); // ARC to x2 - rx, y2

    p.lineTo(x1 + rx, y2);
    p.arcTo(x1, y2 - ry * 2, rx * 2, ry * 2, 270, -90); // ARC to x1, y2 - ry

    p.lineTo(x1, y1 + ry);
    p.arcTo(x1, y1, rx * 2, ry * 2, 180, -90); // ARC to x1 + rx, y1


    handlePathNode(node, p);

    return;
}

void QSvgVisitorImpl::visitEllipseNode(const QSvgEllipse *node)
{
    QRectF rect = node->rect();

    QPainterPath p;
    p.addEllipse(rect);

    handlePathNode(node, p);
}

void QSvgVisitorImpl::visitPathNode(const QSvgPath *node)
{
    handlePathNode(node, node->path());
}

void QSvgVisitorImpl::visitLineNode(const QSvgLine *node)
{
    // TODO: proper end caps (should be flat by default?)
    QPainterPath p;
    p.moveTo(node->line().p1());
    p.lineTo(node->line().p2());
    handlePathNode(node, p, Qt::FlatCap);
}

void QSvgVisitorImpl::visitPolygonNode(const QSvgPolygon *node)
{
    QPainterPath p = QQuickVectorGraphics::Utils::polygonToPath(node->polygon(), true);
    handlePathNode(node, p);
}

void QSvgVisitorImpl::visitPolylineNode(const QSvgPolyline *node)
{
    QPainterPath p = QQuickVectorGraphics::Utils::polygonToPath(node->polygon(), false);
    handlePathNode(node, p, Qt::FlatCap);
}


void QSvgVisitorImpl::visitTextNode(const QSvgText *node)
{
    // TODO: font/size
    // TODO: fallback to path for gradient fill

    handleBaseNodeSetup(node);
    const bool isTextArea = node->type() == QSvgNode::Textarea;

    QString text;
    for (const auto *tspan : node->tspans()) {
        if (!tspan) {
            text += QStringLiteral("<br>");
            continue;
        }

        if (!tspan->style().font.isDefault()) // TODO: switch to rich text when we have more complex spans with fonts?
            qCDebug(lcQuickVectorGraphics) << "Not implemented Tspan with font:" << tspan->style().font->qfont();
        QString spanColor;
        if (!tspan->style().fill.isDefault()) {
            auto &b = tspan->style().fill->qbrush();
            qCDebug(lcQuickVectorGraphics) << "tspan FILL:" << b;
            if (b.style() != Qt::NoBrush)
                spanColor = b.color().name();
        }
        bool fontTag = !spanColor.isEmpty();
        if (fontTag)
            text += QStringLiteral("<font color=\"%1\">").arg(spanColor); // TODO: size="1-7" ???
        text += tspan->text().toHtmlEscaped();
        if (fontTag)
            text += QStringLiteral("</font>");
    }

    QFont font = styleResolver->painter().font();

    if (font.pixelSize() <= 0 && font.pointSize() > 0)
        font.setPixelSize(font.pointSize()); // ### TODO: this makes no sense ###

    TextNodeInfo info;
    fillCommonNodeInfo(node, info);

    info.position = node->position();
    info.size = node->size();
    info.font = font;
    info.text = text;
    info.isTextArea = isTextArea;
    info.color = styleResolver->currentFillColor();
    info.alignment = styleResolver->states().textAnchor;

    m_generator->generateTextNode(info);

    handleBaseNodeEnd(node);
}

bool QSvgVisitorImpl::visitDefsNodeStart(const QSvgDefs *node)
{
    Q_UNUSED(node);
    NodeInfo info;
    fillCommonNodeInfo(node, info);

    m_generator->generateDefsNode(info);

    // TODO CHECK WHAT IS THIS
    return false;
}

bool QSvgVisitorImpl::visitStructureNodeStart(const QSvgStructureNode *node)
{
    constexpr bool forceSeparatePaths = false;
    handleBaseNodeSetup(node);

    StructureNodeInfo info;

    fillCommonNodeInfo(node, info);
    info.forceSeparatePaths = forceSeparatePaths;
    info.isPathContainer = isPathContainer(node);
    info.stage = StructureNodeInfo::StructureNodeStage::Start;

    m_generator->generateStructureNode(info);

    return true;
}

void QSvgVisitorImpl::visitStructureNodeEnd(const QSvgStructureNode *node)
{
    handleBaseNodeEnd(node);
    //    qCDebug(lcQuickVectorGraphics) << "REVERT" << node->nodeId() << node->type() << (m_styleResolver->painter().pen().style() != Qt::NoPen) << m_styleResolver->painter().pen().color().name()
    //             << (m_styleResolver->painter().pen().brush().style() != Qt::NoBrush) << m_styleResolver->painter().pen().brush().color().name();

    StructureNodeInfo info;
    fillCommonNodeInfo(node, info);
    info.stage = StructureNodeInfo::StructureNodeStage::End;

    m_generator->generateStructureNode(info);
}

bool QSvgVisitorImpl::visitDocumentNodeStart(const QSvgTinyDocument *node)
{
    handleBaseNodeSetup(node);

    StructureNodeInfo info;
    fillCommonNodeInfo(node, info);

    const QSvgTinyDocument *doc = static_cast<const QSvgTinyDocument *>(node);
    info.size = doc->size();
    info.viewBox = doc->viewBox();
    info.isPathContainer = isPathContainer(node);
    info.stage = StructureNodeInfo::StructureNodeStage::Start;

    m_generator->generateRootNode(info);

    return true;
}

void QSvgVisitorImpl::visitDocumentNodeEnd(const QSvgTinyDocument *node)
{
    handleBaseNodeEnd(node);
    qCDebug(lcQuickVectorGraphics) << "REVERT" << node->nodeId() << node->type() << (styleResolver->painter().pen().style() != Qt::NoPen)
                                   << styleResolver->painter().pen().color().name() << (styleResolver->painter().pen().brush().style() != Qt::NoBrush)
                                   << styleResolver->painter().pen().brush().color().name();

    StructureNodeInfo info;
    fillCommonNodeInfo(node, info);
    info.stage = StructureNodeInfo::StructureNodeStage::End;

    m_generator->generateRootNode(info);
}

void QSvgVisitorImpl::fillCommonNodeInfo(const QSvgNode *node, NodeInfo &info)
{
    info.nodeId = node->nodeId();
    info.typeName = node->typeName();
    info.isDefaultTransform = node->style().transform.isDefault();
    info.transform = !info.isDefaultTransform ? node->style().transform->qtransform() : QTransform();
    info.isDefaultOpacity = node->style().opacity.isDefault();
    info.opacity = !info.isDefaultOpacity ? node->style().opacity->opacity() : 1.0;
}

void QSvgVisitorImpl::handleBaseNodeSetup(const QSvgNode *node)
{
    qCDebug(lcQuickVectorGraphics) << "Before SETUP" << node << "fill" << styleResolver->currentFillColor()
                                   << "stroke" << styleResolver->currentStrokeColor() << styleResolver->currentStrokeWidth()
                                   << node->nodeId() << " type: " << node->typeName()  << " " << node->type();

    node->applyStyle(&styleResolver->painter(), styleResolver->states());

    qCDebug(lcQuickVectorGraphics) << "After SETUP" << node << "fill" << styleResolver->currentFillColor()
                                   << "stroke" << styleResolver->currentStrokeColor()
                                   << styleResolver->currentStrokeWidth() << node->nodeId();
}

void QSvgVisitorImpl::handleBaseNode(const QSvgNode *node)
{
    NodeInfo info;
    fillCommonNodeInfo(node, info);

    m_generator->generateNodeBase(info);
}

void QSvgVisitorImpl::handleBaseNodeEnd(const QSvgNode *node)
{
    node->revertStyle(&styleResolver->painter(), styleResolver->states());

    qCDebug(lcQuickVectorGraphics) << "After END" << node << "fill" << styleResolver->currentFillColor()
                                   << "stroke" << styleResolver->currentStrokeColor() << styleResolver->currentStrokeWidth()
                                   << node->nodeId();

}

void QSvgVisitorImpl::handlePathNode(const QSvgNode *node, const QPainterPath &path, Qt::PenCapStyle capStyle)
{
    handleBaseNodeSetup(node);

    PathNodeInfo info;
    fillCommonNodeInfo(node, info);
    auto fillStyle = node->style().fill;
    if (fillStyle)
        info.fillRule = fillStyle->fillRule();

    info.painterPath = path;
    info.capStyle = capStyle;
    info.fillColor = styleResolver->currentFillColor();
    info.strokeColor = styleResolver->currentStrokeColor();
    info.strokeWidth = styleResolver->currentStrokeWidth();
    info.grad = styleResolver->currentFillGradient();

    m_generator->generatePath(info);

    handleBaseNodeEnd(node);
}

QT_END_NAMESPACE

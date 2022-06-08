/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QMLTCCOMPILERPIECES_H
#define QMLTCCOMPILERPIECES_H

#include <QtCore/qscopeguard.h>
#include <QtCore/qstringbuilder.h>
#include <QtCore/qfileinfo.h>

#include <private/qqmljsutils_p.h>

#include "qmltcoutputir.h"
#include "qmltcvisitor.h"

QT_BEGIN_NAMESPACE

/*!
    \internal

    Helper class that generates code for the output IR. Takes care of
    complicated, repetitive, nasty logic which is better kept in a single
    confined place.
*/
struct QmltcCodeGenerator
{
    static const QString privateEngineName;
    static const QString typeCountName;

    QString documentUrl;
    QmltcVisitor *visitor = nullptr;

    [[nodiscard]] inline decltype(auto) generate_initCode(QmltcType &current,
                                                          const QQmlJSScope::ConstPtr &type) const;
    inline void generate_initCodeForTopLevelComponent(QmltcType &current,
                                                      const QQmlJSScope::ConstPtr &type);
    [[nodiscard]] inline decltype(auto)
    generate_endInitCode(QmltcType &current, const QQmlJSScope::ConstPtr &type) const;

    inline void generate_interfaceCallCode(QmltcMethod *function, const QQmlJSScope::ConstPtr &type,
                                           const QString &interfaceName,
                                           const QString &interfaceCall) const;
    inline void generate_beginClassCode(QmltcType &current,
                                        const QQmlJSScope::ConstPtr &type) const;
    inline void generate_completeComponentCode(QmltcType &current,
                                               const QQmlJSScope::ConstPtr &type) const;
    inline void generate_finalizeComponentCode(QmltcType &current,
                                               const QQmlJSScope::ConstPtr &type) const;
    inline void generate_handleOnCompletedCode(QmltcType &current,
                                               const QQmlJSScope::ConstPtr &type) const;

    static void generate_assignToProperty(QStringList *block, const QQmlJSScope::ConstPtr &type,
                                          const QQmlJSMetaProperty &p, const QString &value,
                                          const QString &accessor,
                                          bool constructFromQObject = false);
    static void generate_setIdValue(QStringList *block, const QString &context, qsizetype index,
                                    const QString &accessor, const QString &idString);

    inline QString generate_typeCount() const
    {
        return generate_typeCount([](const QQmlJSScope::ConstPtr &) { return false; });
    }
    template<typename Predicate>
    inline QString generate_typeCount(Predicate p) const;

    static void generate_callExecuteRuntimeFunction(QStringList *block, const QString &url,
                                                    QQmlJSMetaMethod::AbsoluteFunctionIndex index,
                                                    const QString &accessor,
                                                    const QString &returnType,
                                                    const QList<QmltcVariable> &parameters = {});

    // TODO: 3 separate versions: bindable QML, bindable C++, non-bindable C++
    static void generate_createBindingOnProperty(QStringList *block, const QString &unitVarName,
                                                 const QString &scope, qsizetype functionIndex,
                                                 const QString &target, int propertyIndex,
                                                 const QQmlJSMetaProperty &p, int valueTypeIndex,
                                                 const QString &subTarget);

    static inline void generate_getCompilationUnitFromUrl();

    struct PreparedValue
    {
        QStringList prologue;
        QString value;
        QStringList epilogue;
    };

    static PreparedValue wrap_mismatchingTypeConversion(const QQmlJSMetaProperty &p, QString value);
    static PreparedValue wrap_extensionType(const QQmlJSScope::ConstPtr &type,
                                            const QQmlJSMetaProperty &p, const QString &accessor);

    static QString wrap_privateClass(const QString &accessor, const QQmlJSMetaProperty &p);
    static QString wrap_qOverload(const QList<QmltcVariable> &parameters,
                                  const QString &overloaded);
    static QString wrap_addressof(const QString &addressed);

    QString urlMethodName() const
    {
        using namespace Qt::StringLiterals;
        QFileInfo fi(documentUrl);
        return u"q_qmltc_docUrl_" + fi.fileName().replace(u".qml"_s, u""_s).replace(u'.', u'_');
    }
};

/*!
    \internal

    Generates \a{current.init}'s code. The init method sets up a QQmlContext for
    the object and (in case \a type is a document root) calls other object
    creation methods in a well-defined order:
    1. current.beginClass
    2. current.endInit
    3. current.completeComponent
    4. current.finalizeComponent
    5. current.handleOnCompleted

    This function returns a QScopeGuard with the final instructions that have to
    be generated at a later point, once everything else is compiled.

    \sa generate_initCodeForTopLevelComponent
*/
inline decltype(auto) QmltcCodeGenerator::generate_initCode(QmltcType &current,
                                                            const QQmlJSScope::ConstPtr &type) const
{
    using namespace Qt::StringLiterals;

    // qmltc_init()'s parameters:
    // * QQmltcObjectCreationHelper* creator
    // * QQmlEngine* engine
    // * const QQmlRefPointer<QQmlContextData>& parentContext
    // * bool canFinalize [optional, when document root]
    const bool isDocumentRoot = type == visitor->result();

    current.init.body << u"Q_UNUSED(creator);"_s; // can happen sometimes

    current.init.body << u"auto context = parentContext;"_s;

    // if parent scope has a QML base type and is not a (current) document root,
    // the parentContext we passed as input to this object is a context of
    // another document. we need to fix it by using parentContext->parent()

    const auto realQmlScope = [](const QQmlJSScope::ConstPtr &scope) {
        if (scope->isArrayScope()) // TODO: it is special for some reason
            return scope->parentScope();
        return scope;
    };

    if (auto parentScope = realQmlScope(type->parentScope());
        parentScope != visitor->result() && QQmlJSUtils::hasCompositeBase(parentScope)) {
        current.init.body << u"// NB: context->parent() is the context of this document"_s;
        current.init.body << u"context = context->parent();"_s;
    }

    // any object with QML base class has to call base's init method
    if (auto base = type->baseType(); base->isComposite()) {
        QString lhs;
        // init creates new context. for document root, it's going to be a real
        // parent context, so store it temporarily in `context` variable
        if (isDocumentRoot)
            lhs = u"context = "_s;
        current.init.body << u"// 0. call base's init method"_s;

        Q_ASSERT(!isDocumentRoot || visitor->qmlTypesWithQmlBases()[0] == visitor->result());
        const auto isCurrentType = [&](const QQmlJSScope::ConstPtr &qmlType) {
            return qmlType == type;
        };
        const QString creationOffset = generate_typeCount(isCurrentType);

        current.init.body << u"{"_s;
        current.init.body << u"QQmltcObjectCreationHelper subCreator(creator, %1);"_s.arg(
                creationOffset);
        current.init.body
                << QStringLiteral("%1%2::%3(&subCreator, engine, context, /* finalize */ false);")
                           .arg(lhs, base->internalName(), current.init.name);
        current.init.body << u"}"_s;
    }

    current.init.body
            << QStringLiteral("auto %1 = QQmlEnginePrivate::get(engine);").arg(privateEngineName);
    current.init.body << QStringLiteral("Q_UNUSED(%1);").arg(privateEngineName); // precaution

    // when generating root, we need to create a new (document-level) context.
    // otherwise, just use existing context as is
    if (isDocumentRoot) {
        current.init.body << u"// 1. create new QML context for this document"_s;
        // TODO: the last 2 parameters are {0, true} because we deal with
        // document root only here. in reality, there are inline components
        // which need { index, true } instead
        current.init.body
                << QStringLiteral(
                           "context = %1->createInternalContext(%1->compilationUnitFromUrl(%2()), "
                           "context, 0, true);")
                           .arg(privateEngineName, urlMethodName());
    } else {
        current.init.body << u"// 1. use current context as this object's context"_s;
        current.init.body << u"// context = context;"_s;
    }

    if (!type->baseType()->isComposite() || isDocumentRoot) {
        current.init.body << u"// 2. set context for this object"_s;
        current.init.body << QStringLiteral(
                                     "%1->setInternalContext(this, context, QQmlContextData::%2);")
                                     .arg(privateEngineName,
                                          (isDocumentRoot ? u"DocumentRoot"_s
                                                          : u"OrdinaryObject"_s));
        if (isDocumentRoot)
            current.init.body << u"context->setContextObject(this);"_s;
    }

    // context is this document's context. we must remember it in each type
    current.variables.emplaceBack(u"QQmlRefPointer<QQmlContextData>"_s, u"q_qmltc_thisContext"_s,
                                  u"nullptr"_s);
    current.init.body << u"%1::q_qmltc_thisContext = context;"_s.arg(type->internalName());

    if (int id = visitor->runtimeId(type); id >= 0) {
        current.init.body << u"// 3. set id since it is provided"_s;
        QString idString = visitor->addressableScopes().id(type);
        if (idString.isEmpty())
            idString = u"<unknown>"_s;
        QmltcCodeGenerator::generate_setIdValue(&current.init.body, u"context"_s, id, u"this"_s,
                                                idString);
    }

    // if type has an extension, create a dynamic meta object for it
    bool hasExtension = false;
    for (auto cppBase = QQmlJSScope::nonCompositeBaseType(type); cppBase;
         cppBase = cppBase->baseType()) {
        // QObject is special: we have a pseudo-extension on it due to builtins
        if (cppBase->internalName() == u"QObject"_s)
            break;
        if (cppBase->extensionType().extensionSpecifier != QQmlJSScope::NotExtension) {
            hasExtension = true;
            break;
        }
    }
    if (hasExtension) {
        current.init.body << u"{"_s;
        current.init.body << u"auto cppData = QmltcTypeData(this);"_s;
        current.init.body
                << QStringLiteral(
                           "qmltcCreateDynamicMetaObject(this, &%1::staticMetaObject, cppData);")
                           .arg(type->internalName());
        current.init.body << u"}"_s;
    }

    const auto generateFinalLines = [&current, isDocumentRoot]() {
        if (isDocumentRoot) {
            current.init.body << u"// 4. finish the document root creation"_s;
            current.init.body << u"if (canFinalize) {"_s;
            current.init.body << QStringLiteral("    %1(creator, /* finalize */ true);")
                                         .arg(current.beginClass.name);
            current.init.body << QStringLiteral("    %1(creator, engine, /* finalize */ true);")
                                         .arg(current.endInit.name);
            current.init.body << QStringLiteral("    %1(creator, /* finalize */ true);")
                                         .arg(current.completeComponent.name);
            current.init.body << QStringLiteral("    %1(creator, /* finalize */ true);")
                                         .arg(current.finalizeComponent.name);
            current.init.body << QStringLiteral("    %1(creator, /* finalize */ true);")
                                         .arg(current.handleOnCompleted.name);
            current.init.body << u"}"_s;
        }
        current.init.body << u"return context;"_s;
    };

    return QScopeGuard(generateFinalLines);
}

/*!
    \internal

    Generates \a{current.init}'s code in case when \a type is a top-level
    Component type. The init method in this case mimics
    QQmlObjectCreator::createComponent() logic.

    \sa generate_initCode
*/
inline void
QmltcCodeGenerator::generate_initCodeForTopLevelComponent(QmltcType &current,
                                                          const QQmlJSScope::ConstPtr &type)
{
    Q_UNUSED(type);

    using namespace Qt::StringLiterals;

    // since we create a document root as QQmlComponent, we only need to fake
    // QQmlComponent construction in init:
    current.init.body << u"// init QQmlComponent: see QQmlObjectCreator::createComponent()"_s;
    current.init.body << u"{"_s;
    // we already called QQmlComponent(parent) constructor. now we need:
    // 1. QQmlComponent(engine, parent) logic:
    current.init.body << u"// QQmlComponent(engine, parent):"_s;
    current.init.body << u"auto d = QQmlComponentPrivate::get(this);"_s;
    current.init.body << u"Q_ASSERT(d);"_s;
    current.init.body << u"d->engine = engine;"_s;
    current.init.body << u"QObject::connect(engine, &QObject::destroyed, this, [d]() {"_s;
    current.init.body << u"    d->state.creator.reset();"_s;
    current.init.body << u"    d->engine = nullptr;"_s;
    current.init.body << u"});"_s;
    // 2. QQmlComponent(engine, compilationUnit, start, parent) logic:
    current.init.body << u"// QQmlComponent(engine, compilationUnit, start, parent):"_s;
    current.init.body
            << u"auto compilationUnit = QQmlEnginePrivate::get(engine)->compilationUnitFromUrl("
                    + QmltcCodeGenerator::urlMethodName() + u"());";
    current.init.body << u"d->compilationUnit = compilationUnit;"_s;
    current.init.body << u"d->start = 0;"_s;
    current.init.body << u"d->url = compilationUnit->finalUrl();"_s;
    current.init.body << u"d->progress = 1.0;"_s;
    // 3. QQmlObjectCreator::createComponent() logic which is left:
    current.init.body << u"// QQmlObjectCreator::createComponent():"_s;
    current.init.body << u"d->creationContext = context;"_s;
    current.init.body << u"Q_ASSERT(QQmlData::get(this, /*create*/ false));"_s;
    current.init.body << u"}"_s;
}

/*!
    \internal

    Generates \a{current.endInit}'s code. The endInit method creates bindings,
    connects signals with slots and generally performs other within-object
    initialization. Additionally, the QML document root's endInit calls endInit
    methods of all the necessary QML types within the document.
*/
inline decltype(auto)
QmltcCodeGenerator::generate_endInitCode(QmltcType &current,
                                         const QQmlJSScope::ConstPtr &type) const
{
    using namespace Qt::StringLiterals;

    // QML_endInit()'s parameters:
    // * QQmltcObjectCreationHelper* creator
    // * QQmlEngine* engine
    // * bool canFinalize [optional, when document root]
    const bool isDocumentRoot = type == visitor->result();
    current.endInit.body << u"Q_UNUSED(creator);"_s;
    current.endInit.body << u"Q_UNUSED(engine);"_s;
    if (isDocumentRoot)
        current.endInit.body << u"Q_UNUSED(canFinalize);"_s;

    if (auto base = type->baseType(); base->isComposite()) {
        current.endInit.body << u"// call base's finalize method"_s;
        Q_ASSERT(!isDocumentRoot || visitor->qmlTypesWithQmlBases()[0] == visitor->result());
        const auto isCurrentType = [&](const QQmlJSScope::ConstPtr &qmlType) {
            return qmlType == type;
        };
        const QString creationOffset = generate_typeCount(isCurrentType);
        current.endInit.body << u"{"_s;
        current.endInit.body << u"QQmltcObjectCreationHelper subCreator(creator, %1);"_s.arg(
                creationOffset);
        current.endInit.body << u"%1::%2(&subCreator, engine, /* finalize */ false);"_s.arg(
                base->internalName(), current.endInit.name);
        current.endInit.body << u"}"_s;
    }

    if (visitor->hasDeferredBindings(type)) {
        current.endInit.body << u"{ // defer bindings"_s;
        current.endInit.body << u"auto ddata = QQmlData::get(this);"_s;
        current.endInit.body << u"auto thisContext = ddata->outerContext;"_s;
        current.endInit.body << u"Q_ASSERT(thisContext);"_s;
        current.endInit.body << QStringLiteral("ddata->deferData(%1, "
                                               "QQmlEnginePrivate::get(engine)->"
                                               "compilationUnitFromUrl(%2()), thisContext);")
                                        .arg(QString::number(visitor->qmlIrObjectIndex(type)),
                                             QmltcCodeGenerator::urlMethodName());
        current.endInit.body << u"}"_s;
    }

    // TODO: QScopeGuard here is redundant. we should call endInit of children
    // directly
    const auto generateFinalLines = [&current, isDocumentRoot, this]() {
        if (!isDocumentRoot) // document root does all the work here
            return;

        const auto types = visitor->pureQmlTypes();
        current.endInit.body << u"// finalize children"_s;
        for (qsizetype i = 1; i < types.size(); ++i) {
            const auto &type = types[i];
            Q_ASSERT(!type->isComponentRootElement());
            current.endInit.body << u"creator->get<%1>(%2)->%3(creator, engine);"_s.arg(
                    type->internalName(), QString::number(i), current.endInit.name);
        }
    };
    return QScopeGuard(generateFinalLines);
}

/*!
    \internal

    A generic helper function that generates interface code boilerplate, adding
    it to a passed \a function. This is a building block used to generate e.g.
    QQmlParserStatus API calls.
*/
inline void QmltcCodeGenerator::generate_interfaceCallCode(QmltcMethod *function,
                                                           const QQmlJSScope::ConstPtr &type,
                                                           const QString &interfaceName,
                                                           const QString &interfaceCall) const
{
    using namespace Qt::StringLiterals;

    // function's parameters:
    // * QQmltcObjectCreationHelper* creator
    // * bool canFinalize [optional, when document root]
    const bool isDocumentRoot = type == visitor->result();
    function->body << u"Q_UNUSED(creator);"_s;
    if (isDocumentRoot)
        function->body << u"Q_UNUSED(canFinalize);"_s;

    if (auto base = type->baseType(); base->isComposite()) {
        function->body << u"// call base's method"_s;
        Q_ASSERT(!isDocumentRoot || visitor->qmlTypesWithQmlBases()[0] == visitor->result());
        const auto isCurrentType = [&](const QQmlJSScope::ConstPtr &qmlType) {
            return qmlType == type;
        };
        const QString creationOffset = generate_typeCount(isCurrentType);
        function->body << u"{"_s;
        function->body << u"QQmltcObjectCreationHelper subCreator(creator, %1);"_s.arg(
                creationOffset);
        function->body << u"%1::%2(&subCreator, /* finalize */ false);"_s.arg(base->internalName(),
                                                                              function->name);
        function->body << u"}"_s;
    }

    if (!isDocumentRoot)
        return;

    const auto types = visitor->pureQmlTypes();
    function->body << u"// call children's methods"_s;
    for (qsizetype i = 1; i < types.size(); ++i) {
        const auto &type = types[i];
        Q_ASSERT(!type->isComponentRootElement());
        function->body << u"{"_s;
        function->body << u"auto child = creator->get<%1>(%2);"_s.arg(type->internalName(),
                                                                      QString::number(i));
        function->body << u"child->%1(creator);"_s.arg(function->name);
        if (type->hasInterface(interfaceName)) {
            function->body << u"Q_ASSERT(dynamic_cast<%1 *>(child) != nullptr);"_s.arg(
                    interfaceName);
            function->body << u"child->%1();"_s.arg(interfaceCall);
        }
        function->body << u"}"_s;
    }

    if (type->hasInterface(interfaceName)) {
        function->body << u"if (canFinalize) {"_s;
        function->body << u"    // call own method"_s;
        function->body << u"    this->%1();"_s.arg(interfaceCall);
        function->body << u"}"_s;
    }
}

/*!
    \internal

    Generates \a{current.beginClass}'s code. The beginClass method optionally
    calls QQmlParserStatus::classBegin() when \a type implements the
    corresponding interface.
*/
inline void QmltcCodeGenerator::generate_beginClassCode(QmltcType &current,
                                                        const QQmlJSScope::ConstPtr &type) const
{
    using namespace Qt::StringLiterals;
    generate_interfaceCallCode(&current.beginClass, type, u"QQmlParserStatus"_s, u"classBegin"_s);
}

/*!
    \internal

    Generates \a{current.completeComponent}'s code. The completeComponent method
    optionally calls QQmlParserStatus::componentComplete() when \a type
    implements the corresponding interface.
*/
inline void
QmltcCodeGenerator::generate_completeComponentCode(QmltcType &current,
                                                   const QQmlJSScope::ConstPtr &type) const
{
    using namespace Qt::StringLiterals;
    generate_interfaceCallCode(&current.completeComponent, type, u"QQmlParserStatus"_s,
                               u"componentComplete"_s);
}

/*!
    \internal

    Generates \a{current.finalizeComponent}'s code. The finalizeComponent method
    optionally calls QQmlFinalizerHook::componentFinalized() when \a type
    implements the corresponding interface.
*/
inline void
QmltcCodeGenerator::generate_finalizeComponentCode(QmltcType &current,
                                                   const QQmlJSScope::ConstPtr &type) const
{
    using namespace Qt::StringLiterals;
    generate_interfaceCallCode(&current.finalizeComponent, type, u"QQmlFinalizerHook"_s,
                               u"componentFinalized"_s);
}

/*!
    \internal

    Generates \a{current.handleOnCompleted}'s code. The handleOnCompleted method
    optionally calls a Component.onCompleted handler if that is present in \a
    type.
*/
inline void
QmltcCodeGenerator::generate_handleOnCompletedCode(QmltcType &current,
                                                   const QQmlJSScope::ConstPtr &type) const
{
    using namespace Qt::StringLiterals;

    // QML_handleOnCompleted()'s parameters:
    // * QQmltcObjectCreationHelper* creator
    // * bool canFinalize [optional, when document root]
    const bool isDocumentRoot = type == visitor->result();
    current.handleOnCompleted.body << u"Q_UNUSED(creator);"_s;
    if (isDocumentRoot)
        current.handleOnCompleted.body << u"Q_UNUSED(canFinalize);"_s;

    if (auto base = type->baseType(); base->isComposite()) {
        current.handleOnCompleted.body << u"// call base's method"_s;
        Q_ASSERT(!isDocumentRoot || visitor->qmlTypesWithQmlBases()[0] == visitor->result());
        const auto isCurrentType = [&](const QQmlJSScope::ConstPtr &qmlType) {
            return qmlType == type;
        };
        const QString creationOffset = generate_typeCount(isCurrentType);
        current.handleOnCompleted.body << u"{"_s;
        current.handleOnCompleted.body
                << u"QQmltcObjectCreationHelper subCreator(creator, %1);"_s.arg(creationOffset);
        current.handleOnCompleted.body << u"%1::%2(&subCreator, /* finalize */ false);"_s.arg(
                base->internalName(), current.handleOnCompleted.name);
        current.handleOnCompleted.body << u"}"_s;
    }

    if (!isDocumentRoot) // document root does all the work here
        return;

    const auto types = visitor->pureQmlTypes();
    current.handleOnCompleted.body << u"// call children's methods"_s;
    for (qsizetype i = 1; i < types.size(); ++i) {
        const auto &type = types[i];
        Q_ASSERT(!type->isComponentRootElement());
        current.handleOnCompleted.body << u"creator->get<%1>(%2)->%3(creator);"_s.arg(
                type->internalName(), QString::number(i), current.handleOnCompleted.name);
    }
}

/*!
    \internal

    Generates a constexpr function consisting of a sum of type counts for a
    current QML document. Predicate \a p acts as a stop condition to prematurely
    end the sum generation.

    The high-level idea:

    Each qmltc-compiled document root has a notion of type count. Type count is
    a number of types the current QML document contains (except for
    Component-wrapped types) plus the sum of all type counts of all the QML
    documents used in the current document: if current document has a type with
    QML base type, this type's type count is added to the type count of the
    current document.

    To be able to lookup created objects during the creation process, one needs
    to know an index of each object within the document + an offset of the
    document. Index comes from QmltcVisitor and is basically a serial number of
    a type in the document (index < type count of the document root type). The
    offset is more indirect.

    The current document always starts with an offset of 0, each type that has a
    QML base type also "has a sub-document". Each sub-document has a non-0
    offset X, where X is calculated as a sum of the current document's type
    count and a cumulative type count of all the previous sub-documents that
    appear before the sub-document of interest:

    \code
    // A.qml
    Item {  // offset: 0; number of types == 1 (document root) + 3 (children)

        QmlBase1 { } // offset: 4 (number of types in A.qml itself)

        QmlBase2 { } // offset: 4 + N, where N == typeCount(QmlBase1.qml)

        QmlBase3 { } // offset: (4 + N) + M, where M == typeCount(QmlBase2.qml)

    } // typeCount(A.qml) == 4 + N + M + O, where O == typeCount(QmlBase3.qml)
    \endcode

    As all objects are put into an array, schematically you can look at it in
    the following way:

    ```
    count:       4         N          M     O
    objects:    aaaa|xxxxxxxxxxxxx|yyyyyyy|zzz
                ^    ^             ^       ^
    files:      |    QmlBase1.qml  |       QmlBase3.qml
                A.qml              QmlBase2.qml
    ```

    For the object lookup logic itself, see QQmltcObjectCreationHelper
*/
template<typename Predicate>
inline QString QmltcCodeGenerator::generate_typeCount(Predicate p) const
{
    using namespace Qt::StringLiterals;

    const QList<QQmlJSScope::ConstPtr> typesWithBaseTypeCount = visitor->qmlTypesWithQmlBases();
    QStringList components;
    components.reserve(1 + typesWithBaseTypeCount.size());

    // add this document's type counts minus document root
    Q_ASSERT(visitor->pureQmlTypes().size() > 0);
    components << QString::number(visitor->pureQmlTypes().size() - 1);

    // traverse types with QML base classes
    for (const QQmlJSScope::ConstPtr &t : typesWithBaseTypeCount) {
        if (p(t))
            break;
        QString typeCountTemplate = u"QQmltcObjectCreationHelper::typeCount<%1>()"_s;
        if (t == visitor->result()) { // t is this document's root
            components << typeCountTemplate.arg(t->baseTypeName());
        } else {
            components << typeCountTemplate.arg(t->internalName());
        }
    }

    return components.join(u" + "_s);
}

QT_END_NAMESPACE

#endif // QMLTCCOMPILERPIECES_H

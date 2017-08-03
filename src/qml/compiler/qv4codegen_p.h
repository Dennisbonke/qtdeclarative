/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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
#ifndef QV4CODEGEN_P_H
#define QV4CODEGEN_P_H

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

#include "private/qv4global_p.h"
#include <private/qqmljsastvisitor_p.h>
#include <private/qqmljsast_p.h>
#include <private/qqmljsengine_p.h>
#include <private/qv4instr_moth_p.h>
#include <private/qv4compiler_p.h>
#include <private/qv4compilercontext_p.h>
#include <private/qqmlrefcount_p.h>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>
#include <QStack>
#ifndef V4_BOOTSTRAP
#include <qqmlerror.h>
#endif
#include <private/qv4util_p.h>
#include <private/qv4bytecodegenerator_p.h>

QT_BEGIN_NAMESPACE

using namespace QQmlJS;

namespace QV4 {

namespace Moth {
struct Instruction;
}

namespace CompiledData {
struct CompilationUnit;
}

namespace Compiler {

struct ControlFlow;
struct ControlFlowCatch;
struct ControlFlowFinally;

class Q_QML_PRIVATE_EXPORT Codegen: protected QQmlJS::AST::Visitor
{
protected:
    using BytecodeGenerator = QV4::Moth::BytecodeGenerator;
    using Instruction = QV4::Moth::Instruction;
public:
    Codegen(QV4::Compiler::JSUnitGenerator *jsUnitGenerator, bool strict);


    void generateFromProgram(const QString &fileName,
                             const QString &sourceCode,
                             AST::Program *ast,
                             Module *module,
                             CompilationMode mode = GlobalCode);

public:
    class RValue {
        Codegen *codegen;
        enum Type {
            Invalid,
            Accumulator,
            StackSlot,
            Const
        } type;
        union {
            Moth::StackSlot theStackSlot;
            QV4::ReturnedValue constant;
        };

    public:
        static RValue fromStackSlot(Codegen *codegen, Moth::StackSlot stackSlot) {
            RValue r;
            r.codegen = codegen;
            r.type = StackSlot;
            r.theStackSlot = stackSlot;
            return r;
        }
        static RValue fromAccumulator(Codegen *codegen) {
            RValue r;
            r.codegen = codegen;
            r.type = Accumulator;
            return r;
        }
        static RValue fromConst(Codegen *codegen, QV4::ReturnedValue value) {
            RValue r;
            r.codegen = codegen;
            r.type = Const;
            r.constant = value;
            return r;
        }

        bool operator==(const RValue &other) const;

        bool isValid() const { return type != Invalid; }
        bool isAccumulator() const { return type == Accumulator; }
        bool isStackSlot() const { return type == StackSlot; }
        bool isConst() const { return type == Const; }

        Moth::StackSlot stackSlot() const {
            Q_ASSERT(isStackSlot());
            return theStackSlot;
        }

        QV4::ReturnedValue constantValue() const {
            Q_ASSERT(isConst());
            return constant;
        }

        Q_REQUIRED_RESULT RValue storeInTemp() const;
    };
    struct Reference {
        enum Type {
            Invalid,
            Accumulator,
            StackSlot,
            Local,
            Argument,
            Name,
            Member,
            Subscript,
            Closure,
            QmlScopeObject,
            QmlContextObject,
            LastLValue = QmlContextObject,
            Const,
            This
        } type = Invalid;

        bool isLValue() const { return !isReadonly; }

        Reference(Codegen *cg, Type type = Invalid) : type(type), codegen(cg) {}
        Reference()
            : type(Invalid)
            , codegen(nullptr)
        {}
        Reference(const Reference &other);

        Reference &operator =(const Reference &other);

        bool operator==(const Reference &other) const;

        bool isValid() const { return type != Invalid; }
        bool loadTriggersSideEffect() const {
            switch (type) {
            case Name:
            case Member:
            case Subscript:
                return true;
            default:
                return false;
            }
        }
        bool isConst() const { return type == Const; }
        bool isAccumulator() const { return type == Accumulator; }
        bool isStackSlot() const { return type == StackSlot; }

        static Reference fromAccumulator(Codegen *cg) {
            return Reference(cg, Accumulator);
        }
        static Reference fromStackSlot(Codegen *cg, int tempIndex = -1, bool isLocal = false) {
            Reference r(cg, StackSlot);
            if (tempIndex == -1)
                tempIndex = cg->bytecodeGenerator->newTemp();
            r.theStackSlot = Moth::StackSlot::create(tempIndex);
            r.stackSlotIsLocal = isLocal;
            return r;
        }
        static Reference fromLocal(Codegen *cg, uint index, uint scope) {
            Reference r(cg, Local);
            r.index = index;
            r.scope = scope;
            return r;
        }
        static Reference fromArgument(Codegen *cg, uint index, uint scope) {
            Reference r(cg, Argument);
            r.index = index;
            r.scope = scope;
            return r;
        }
        static Reference fromName(Codegen *cg, const QString &name) {
            Reference r(cg, Name);
            r.unqualifiedNameIndex = cg->registerString(name);
            return r;
        }
        static Reference fromMember(const Reference &baseRef, const QString &name) {
            Reference r(baseRef.codegen, Member);
            r.propertyBase = baseRef.asRValue();
            r.propertyNameIndex = r.codegen->registerString(name);
            return r;
        }
        static Reference fromSubscript(const Reference &baseRef, const Reference &subscript) {
            Q_ASSERT(baseRef.isStackSlot());
            Reference r(baseRef.codegen, Subscript);
            r.elementBase = baseRef.stackSlot();
            r.elementSubscript = subscript.asRValue();
            return r;
        }
        static Reference fromConst(Codegen *cg, QV4::ReturnedValue constant) {
            Reference r(cg, Const);
            r.constant = constant;
            r.isReadonly = true;
            return r;
        }
        static Reference fromClosure(Codegen *cg, int functionId) {
            Reference r(cg, Closure);
            r.closureId = functionId;
            return r;
        }
        static Reference fromQmlScopeObject(const Reference &base, qint16 coreIndex, qint16 notifyIndex, bool captureRequired) {
            Reference r(base.codegen, QmlScopeObject);
            r.qmlBase = base.storeOnStack().stackSlot();
            r.qmlCoreIndex = coreIndex;
            r.qmlNotifyIndex = notifyIndex;
            r.captureRequired = captureRequired;
            return r;
        }
        static Reference fromQmlContextObject(const Reference &base, qint16 coreIndex, qint16 notifyIndex, bool captureRequired) {
            Reference r(base.codegen, QmlContextObject);
            r.qmlBase = base.storeOnStack().stackSlot();
            r.qmlCoreIndex = coreIndex;
            r.qmlNotifyIndex = notifyIndex;
            r.captureRequired = captureRequired;
            return r;
        }
        static Reference fromThis(Codegen *cg) {
            Reference r(cg, This);
            r.isReadonly = true;
            return r;
        }

        RValue asRValue() const;
        Reference asLValue() const;
        static Reference storeConstOnStack(Codegen *cg, QV4::ReturnedValue constant, int stackSlot = -1)
        { return Reference::fromConst(cg, constant).storeOnStack(stackSlot); }
        Q_REQUIRED_RESULT Reference storeOnStack(int tempIndex = -1) const;
        Q_REQUIRED_RESULT Reference storeRetainAccumulator() const;
        Reference storeConsumeAccumulator() const;

        bool storeWipesAccumulator() const;
        void loadInAccumulator() const;

        Moth::StackSlot stackSlot() const {
            if (Q_UNLIKELY(!isStackSlot()))
                Q_UNREACHABLE();
            return theStackSlot;
        }

        union {
            Moth::StackSlot theStackSlot;
            QV4::ReturnedValue constant;
            int unqualifiedNameIndex;
            struct { // Argument/Local
                int index;
                int scope;
            };
            struct {
                RValue propertyBase;
                int propertyNameIndex;
            };
            struct {
                Moth::StackSlot elementBase;
                RValue elementSubscript;
            };
            int closureId;
            struct { // QML scope/context object case
                Moth::StackSlot qmlBase;
                qint16 qmlCoreIndex;
                qint16 qmlNotifyIndex;
                bool captureRequired;
            };
        };
        mutable bool isArgOrEval = false;
        bool isReadonly = false;
        bool stackSlotIsLocal = false;
        bool global = false;
        Codegen *codegen;

    private:
        void storeAccumulator() const;
    };

    struct TempScope {
        TempScope(Codegen *cg)
            : generator(cg->bytecodeGenerator),
              tempCountForScope(generator->currentTemp) {}
        ~TempScope() {
            generator->currentTemp = tempCountForScope;
        }
        BytecodeGenerator *generator;
        int tempCountForScope;
    };

    struct ObjectPropertyValue {
        ObjectPropertyValue()
            : getter(-1)
            , setter(-1)
            , keyAsIndex(UINT_MAX)
        {}

        Reference rvalue;
        int getter; // index in _module->functions or -1 if not set
        int setter;
        uint keyAsIndex;

        bool hasGetter() const { return getter >= 0; }
        bool hasSetter() const { return setter >= 0; }
    };
protected:

    enum Format { ex, cx, nx };
    class Result {
        Reference _result;

        const BytecodeGenerator::Label *_iftrue;
        const BytecodeGenerator::Label *_iffalse;
        Format _format;
        Format _requested;
        bool _trueBlockFollowsCondition = false;

    public:
        explicit Result(const Reference &lrvalue)
            : _result(lrvalue)
            , _iftrue(nullptr)
            , _iffalse(nullptr)
            , _format(ex)
            , _requested(ex)
        {
        }

        explicit Result(Format requested = ex)
            : _iftrue(0)
            , _iffalse(0)
            , _format(ex)
            , _requested(requested) {}

        explicit Result(const BytecodeGenerator::Label *iftrue,
                        const BytecodeGenerator::Label *iffalse,
                        bool trueBlockFollowsCondition)
            : _iftrue(iftrue)
            , _iffalse(iffalse)
            , _format(ex)
            , _requested(cx)
            , _trueBlockFollowsCondition(trueBlockFollowsCondition)
        {
            Q_ASSERT(iftrue);
            Q_ASSERT(iffalse);
        }

        const BytecodeGenerator::Label *iftrue() const {
            Q_ASSERT(_requested == cx);
            return _iftrue;
        }

        const BytecodeGenerator::Label *iffalse() const {
            Q_ASSERT(_requested == cx);
            return _iffalse;
        }

        Format format() const {
            return _format;
        }

        bool accept(Format f)
        {
            if (_requested == f) {
                _format = f;
                return true;
            }
            return false;
        }

        bool trueBlockFollowsCondition() const {
            return _trueBlockFollowsCondition;
        }

        const Reference &result() const {
            return _result;
        }

        void setResult(const Reference &result) {
            _result = result;
        }
    };

    void enterContext(AST::Node *node);
    int leaveContext();

    void leaveLoop();

    enum UnaryOperation {
        UPlus,
        UMinus,
        PreIncrement,
        PreDecrement,
        PostIncrement,
        PostDecrement,
        Not,
        Compl
    };

    Reference unop(UnaryOperation op, const Reference &expr);

    int registerString(const QString &name) {
        return jsUnitGenerator->registerString(name);
    }
    int registerConstant(QV4::ReturnedValue v) { return jsUnitGenerator->registerConstant(v); }
    int registerGetterLookup(int nameIndex) { return jsUnitGenerator->registerGetterLookup(nameIndex); }
    int registerSetterLookup(int nameIndex) { return jsUnitGenerator->registerSetterLookup(nameIndex); }
    int registerGlobalGetterLookup(int nameIndex) { return jsUnitGenerator->registerGlobalGetterLookup(nameIndex); }

    // Returns index in _module->functions
    virtual int defineFunction(const QString &name, AST::Node *ast,
                               AST::FormalParameterList *formals,
                               AST::SourceElements *body);

    void statement(AST::Statement *ast);
    void statement(AST::ExpressionNode *ast);
    void condition(AST::ExpressionNode *ast, const BytecodeGenerator::Label *iftrue,
                   const BytecodeGenerator::Label *iffalse,
                   bool trueBlockFollowsCondition);
    Reference expression(AST::ExpressionNode *ast);
    Result sourceElement(AST::SourceElement *ast);

    void accept(AST::Node *node);

    void functionBody(AST::FunctionBody *ast);
    void program(AST::Program *ast);
    void sourceElements(AST::SourceElements *ast);
    void variableDeclaration(AST::VariableDeclaration *ast);
    void variableDeclarationList(AST::VariableDeclarationList *ast);

    Reference referenceForName(const QString &name, bool lhs);

    // Hook provided to implement QML lookup semantics
    virtual Reference fallbackNameLookup(const QString &name);
    virtual void beginFunctionBodyHook() {}

    // nodes
    bool visit(AST::ArgumentList *ast) override;
    bool visit(AST::CaseBlock *ast) override;
    bool visit(AST::CaseClause *ast) override;
    bool visit(AST::CaseClauses *ast) override;
    bool visit(AST::Catch *ast) override;
    bool visit(AST::DefaultClause *ast) override;
    bool visit(AST::ElementList *ast) override;
    bool visit(AST::Elision *ast) override;
    bool visit(AST::Finally *ast) override;
    bool visit(AST::FormalParameterList *ast) override;
    bool visit(AST::FunctionBody *ast) override;
    bool visit(AST::Program *ast) override;
    bool visit(AST::PropertyNameAndValue *ast) override;
    bool visit(AST::PropertyAssignmentList *ast) override;
    bool visit(AST::PropertyGetterSetter *ast) override;
    bool visit(AST::SourceElements *ast) override;
    bool visit(AST::StatementList *ast) override;
    bool visit(AST::UiArrayMemberList *ast) override;
    bool visit(AST::UiImport *ast) override;
    bool visit(AST::UiHeaderItemList *ast) override;
    bool visit(AST::UiPragma *ast) override;
    bool visit(AST::UiObjectInitializer *ast) override;
    bool visit(AST::UiObjectMemberList *ast) override;
    bool visit(AST::UiParameterList *ast) override;
    bool visit(AST::UiProgram *ast) override;
    bool visit(AST::UiQualifiedId *ast) override;
    bool visit(AST::UiQualifiedPragmaId *ast) override;
    bool visit(AST::VariableDeclaration *ast) override;
    bool visit(AST::VariableDeclarationList *ast) override;

    // expressions
    bool visit(AST::Expression *ast) override;
    bool visit(AST::ArrayLiteral *ast) override;
    bool visit(AST::ArrayMemberExpression *ast) override;
    bool visit(AST::BinaryExpression *ast) override;
    bool visit(AST::CallExpression *ast) override;
    bool visit(AST::ConditionalExpression *ast) override;
    bool visit(AST::DeleteExpression *ast) override;
    bool visit(AST::FalseLiteral *ast) override;
    bool visit(AST::FieldMemberExpression *ast) override;
    bool visit(AST::FunctionExpression *ast) override;
    bool visit(AST::IdentifierExpression *ast) override;
    bool visit(AST::NestedExpression *ast) override;
    bool visit(AST::NewExpression *ast) override;
    bool visit(AST::NewMemberExpression *ast) override;
    bool visit(AST::NotExpression *ast) override;
    bool visit(AST::NullExpression *ast) override;
    bool visit(AST::NumericLiteral *ast) override;
    bool visit(AST::ObjectLiteral *ast) override;
    bool visit(AST::PostDecrementExpression *ast) override;
    bool visit(AST::PostIncrementExpression *ast) override;
    bool visit(AST::PreDecrementExpression *ast) override;
    bool visit(AST::PreIncrementExpression *ast) override;
    bool visit(AST::RegExpLiteral *ast) override;
    bool visit(AST::StringLiteral *ast) override;
    bool visit(AST::ThisExpression *ast) override;
    bool visit(AST::TildeExpression *ast) override;
    bool visit(AST::TrueLiteral *ast) override;
    bool visit(AST::TypeOfExpression *ast) override;
    bool visit(AST::UnaryMinusExpression *ast) override;
    bool visit(AST::UnaryPlusExpression *ast) override;
    bool visit(AST::VoidExpression *ast) override;
    bool visit(AST::FunctionDeclaration *ast) override;

    // source elements
    bool visit(AST::FunctionSourceElement *ast) override;
    bool visit(AST::StatementSourceElement *ast) override;

    // statements
    bool visit(AST::Block *ast) override;
    bool visit(AST::BreakStatement *ast) override;
    bool visit(AST::ContinueStatement *ast) override;
    bool visit(AST::DebuggerStatement *ast) override;
    bool visit(AST::DoWhileStatement *ast) override;
    bool visit(AST::EmptyStatement *ast) override;
    bool visit(AST::ExpressionStatement *ast) override;
    bool visit(AST::ForEachStatement *ast) override;
    bool visit(AST::ForStatement *ast) override;
    bool visit(AST::IfStatement *ast) override;
    bool visit(AST::LabelledStatement *ast) override;
    bool visit(AST::LocalForEachStatement *ast) override;
    bool visit(AST::LocalForStatement *ast) override;
    bool visit(AST::ReturnStatement *ast) override;
    bool visit(AST::SwitchStatement *ast) override;
    bool visit(AST::ThrowStatement *ast) override;
    bool visit(AST::TryStatement *ast) override;
    bool visit(AST::VariableStatement *ast) override;
    bool visit(AST::WhileStatement *ast) override;
    bool visit(AST::WithStatement *ast) override;

    // ui object members
    bool visit(AST::UiArrayBinding *ast) override;
    bool visit(AST::UiObjectBinding *ast) override;
    bool visit(AST::UiObjectDefinition *ast) override;
    bool visit(AST::UiPublicMember *ast) override;
    bool visit(AST::UiScriptBinding *ast) override;
    bool visit(AST::UiSourceElement *ast) override;

    bool throwSyntaxErrorOnEvalOrArgumentsInStrictMode(const Reference &r, const AST::SourceLocation &loc);
    virtual void throwSyntaxError(const AST::SourceLocation &loc, const QString &detail);
    virtual void throwReferenceError(const AST::SourceLocation &loc, const QString &detail);

public:
    QList<DiagnosticMessage> errors() const;
#ifndef V4_BOOTSTRAP
    QList<QQmlError> qmlErrors() const;
#endif

    Reference binopHelper(QSOperator::Op oper, Reference &left, Reference &right);
    Reference jumpBinop(QSOperator::Op oper, Reference &left, Reference &right);
    Moth::StackSlot pushArgs(AST::ArgumentList *args);

    void setUseFastLookups(bool b) { useFastLookups = b; }

    void handleTryCatch(AST::TryStatement *ast);
    void handleTryFinally(AST::TryStatement *ast);

    QQmlRefPointer<QV4::CompiledData::CompilationUnit> generateCompilationUnit(bool generateUnitData = true);
    static QQmlRefPointer<QV4::CompiledData::CompilationUnit> createUnitForLoading();

protected:
    friend class ScanFunctions;
    friend struct ControlFlow;
    friend struct ControlFlowCatch;
    friend struct ControlFlowFinally;
    Result _expr;
    Module *_module;
    BytecodeGenerator::Label _exitBlock;
    int _returnAddress;
    Context *_context;
    AST::LabelledStatement *_labelledStatement;
    QV4::Compiler::JSUnitGenerator *jsUnitGenerator;
    BytecodeGenerator *bytecodeGenerator = 0;
    bool _strictMode;
    bool useFastLookups = true;

    bool _fileNameIsUrl;
    bool hasError;
    QList<QQmlJS::DiagnosticMessage> _errors;
};

}

}

QT_END_NAMESPACE

#endif // QV4CODEGEN_P_H

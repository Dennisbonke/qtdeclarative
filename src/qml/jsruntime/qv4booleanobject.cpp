/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qv4booleanobject_p.h"

using namespace QV4;

DEFINE_MANAGED_VTABLE(BooleanCtor);

BooleanCtor::BooleanCtor(ExecutionContext *scope)
    : FunctionObject(scope, scope->engine->newIdentifier("Boolean"))
{
    vtbl = &static_vtbl;
}

ReturnedValue BooleanCtor::construct(Managed *m, CallData *callData)
{
    Scope scope(m->engine());
    bool n = callData->argc ? callData->args[0].toBoolean() : false;
    ScopedValue b(scope, QV4::Value::fromBoolean(n));
    return Encode(m->engine()->newBooleanObject(b));
}

ReturnedValue BooleanCtor::call(Managed *, CallData *callData)
{
    bool value = callData->argc ? callData->args[0].toBoolean() : 0;
    return Encode(value);
}

void BooleanPrototype::init(ExecutionEngine *engine, const Value &ctor)
{
    ctor.objectValue()->defineReadonlyProperty(engine->id_length, Value::fromInt32(1));
    ctor.objectValue()->defineReadonlyProperty(engine->id_prototype, Value::fromObject(this));
    defineDefaultProperty(QStringLiteral("constructor"), ctor);
    defineDefaultProperty(engine->id_toString, method_toString);
    defineDefaultProperty(engine->id_valueOf, method_valueOf);
}

ReturnedValue BooleanPrototype::method_toString(SimpleCallContext *ctx)
{
    bool result;
    if (ctx->thisObject.isBoolean()) {
        result = ctx->thisObject.booleanValue();
    } else {
        BooleanObject *thisObject = ctx->thisObject.asBooleanObject();
        if (!thisObject)
            ctx->throwTypeError();
        result = thisObject->value.booleanValue();
    }

    return Value::fromString(ctx, QLatin1String(result ? "true" : "false")).asReturnedValue();
}

ReturnedValue BooleanPrototype::method_valueOf(SimpleCallContext *ctx)
{
    BooleanObject *thisObject = ctx->thisObject.asBooleanObject();
    if (!thisObject)
        ctx->throwTypeError();

    return thisObject->value.asReturnedValue();
}

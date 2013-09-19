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

#include "qv4string_p.h"
#include "qv4identifiertable_p.h"
#include "qv4runtime_p.h"
#include "qv4objectproto_p.h"
#include "qv4stringobject_p.h"
#include <QtCore/QHash>

using namespace QV4;

static uint toArrayIndex(const QChar *ch, const QChar *end, bool *ok)
{
    *ok = false;
    uint i = ch->unicode() - '0';
    if (i > 9)
        return UINT_MAX;
    ++ch;
    // reject "01", "001", ...
    if (i == 0 && ch != end)
        return UINT_MAX;

    while (ch < end) {
        uint x = ch->unicode() - '0';
        if (x > 9)
            return UINT_MAX;
        uint n = i*10 + x;
        if (n < i)
            // overflow
            return UINT_MAX;
        i = n;
        ++ch;
    }
    *ok = true;
    return i;
}

static uint toArrayIndex(const char *ch, const char *end, bool *ok)
{
    *ok = false;
    uint i = *ch - '0';
    if (i > 9)
        return UINT_MAX;
    ++ch;
    // reject "01", "001", ...
    if (i == 0 && ch != end)
        return UINT_MAX;

    while (ch < end) {
        uint x = *ch - '0';
        if (x > 9)
            return UINT_MAX;
        uint n = i*10 + x;
        if (n < i)
            // overflow
            return UINT_MAX;
        i = n;
        ++ch;
    }
    *ok = true;
    return i;
}


const ManagedVTable String::static_vtbl =
{
    call,
    construct,
    0 /*markObjects*/,
    destroy,
    0 /*collectDeletables*/,
    hasInstance,
    get,
    getIndexed,
    put,
    putIndexed,
    query,
    queryIndexed,
    deleteProperty,
    deleteIndexedProperty,
    0 /*getLookup*/,
    0 /*setLookup*/,
    isEqualTo,
    0 /*advanceIterator*/,
    "String",
};

void String::destroy(Managed *that)
{
    static_cast<String*>(that)->~String();
}

ReturnedValue String::get(Managed *m, const StringRef name, bool *hasProperty)
{
    ExecutionEngine *v4 = m->engine();
    Scope scope(v4);
    ScopedString that(scope, static_cast<String *>(m));

    if (name->isEqualTo(v4->id_length)) {
        if (hasProperty)
            *hasProperty = true;
        return Value::fromInt32(that->_text.length()).asReturnedValue();
    }
    PropertyAttributes attrs;
    Property *pd = v4->stringClass->prototype->__getPropertyDescriptor__(name, &attrs);
    if (!pd || attrs.isGeneric()) {
        if (hasProperty)
            *hasProperty = false;
        return Value::undefinedValue().asReturnedValue();
    }
    if (hasProperty)
        *hasProperty = true;
    return v4->stringClass->prototype->getValue(that, pd, attrs);
}

ReturnedValue String::getIndexed(Managed *m, uint index, bool *hasProperty)
{
    ExecutionEngine *engine = m->engine();
    Scope scope(engine);
    ScopedString that(scope, static_cast<String *>(m));

    if (index < that->_text.length()) {
        if (hasProperty)
            *hasProperty = true;
        return Value::fromString(engine->newString(that->toQString().mid(index, 1))).asReturnedValue();
    }
    PropertyAttributes attrs;
    Property *pd = engine->stringClass->prototype->__getPropertyDescriptor__(index, &attrs);
    if (!pd || attrs.isGeneric()) {
        if (hasProperty)
            *hasProperty = false;
        return Value::undefinedValue().asReturnedValue();
    }
    if (hasProperty)
        *hasProperty = true;
    return engine->stringClass->prototype->getValue(that, pd, attrs);
}

void String::put(Managed *m, const StringRef name, const ValueRef value)
{
    Scope scope(m->engine());
    String *that = static_cast<String *>(m);
    Scoped<Object> o(scope, that->engine()->newStringObject(Value::fromString(that)));
    o->put(name, value);
}

void String::putIndexed(Managed *m, uint index, const ValueRef value)
{
    Scope scope(m->engine());
    String *that = static_cast<String *>(m);
    Scoped<Object> o(scope, that->engine()->newStringObject(Value::fromString(that)));
    o->putIndexed(index, value);
}

PropertyAttributes String::query(const Managed *m, StringRef name)
{
    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return queryIndexed(m, idx);
    return Attr_Invalid;
}

PropertyAttributes String::queryIndexed(const Managed *m, uint index)
{
    const String *that = static_cast<const String *>(m);
    return (index < that->_text.length()) ? Attr_NotConfigurable|Attr_NotWritable : Attr_Invalid;
}

bool String::deleteProperty(Managed *, const StringRef)
{
    return false;
}

bool String::deleteIndexedProperty(Managed *m, uint index)
{
    return false;
}

bool String::isEqualTo(Managed *t, Managed *o)
{
    if (t == o)
        return true;

    Q_ASSERT(t->type == Type_String && o->type == Type_String);
    String *that = static_cast<String *>(t);
    String *other = static_cast<String *>(o);
    if (that->hashValue() != other->hashValue())
        return false;
    if (that->identifier && that->identifier == other->identifier)
        return true;
    if (that->subtype >= StringType_UInt && that->subtype == other->subtype)
        return true;

    return that->toQString() == other->toQString();
}


String::String(ExecutionEngine *engine, const QString &text)
    : Managed(engine ? engine->emptyClass : 0), _text(text), identifier(0), stringHash(UINT_MAX)
{
    vtbl = &static_vtbl;
    type = Type_String;
    subtype = StringType_Unknown;
}

uint String::toUInt(bool *ok) const
{
    *ok = true;

    if (subtype == StringType_Unknown)
        createHashValue();
    if (subtype >= StringType_UInt)
        return stringHash;

    // ### this conversion shouldn't be required
    double d = __qmljs_string_to_number(toQString());
    uint l = (uint)d;
    if (d == l)
        return l;
    *ok = false;
    return UINT_MAX;
}

void String::makeIdentifierImpl() const
{
    engine()->identifierTable->identifier(this);
}

void String::createHashValue() const
{
    const QChar *ch = _text.constData();
    const QChar *end = ch + _text.length();

    // array indices get their number as hash value
    bool ok;
    stringHash = toArrayIndex(ch, end, &ok);
    if (ok) {
        subtype = (stringHash == UINT_MAX) ? StringType_UInt : StringType_ArrayIndex;
        return;
    }

    uint h = 0xffffffff;
    while (ch < end) {
        h = 31 * h + ch->unicode();
        ++ch;
    }

    stringHash = h;
    subtype = StringType_Regular;
}

uint String::createHashValue(const QChar *ch, int length)
{
    const QChar *end = ch + length;

    // array indices get their number as hash value
    bool ok;
    uint stringHash = toArrayIndex(ch, end, &ok);
    if (ok)
        return stringHash;

    uint h = 0xffffffff;
    while (ch < end) {
        h = 31 * h + ch->unicode();
        ++ch;
    }

    return h;
}

uint String::createHashValue(const char *ch, int length)
{
    const char *end = ch + length;

    // array indices get their number as hash value
    bool ok;
    uint stringHash = toArrayIndex(ch, end, &ok);
    if (ok)
        return stringHash;

    uint h = 0xffffffff;
    while (ch < end) {
        if (*ch >= 0x80)
            return UINT_MAX;
        h = 31 * h + *ch;
        ++ch;
    }

    return h;
}

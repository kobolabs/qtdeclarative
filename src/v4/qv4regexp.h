/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the V4VM module of the Qt Toolkit.
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
#ifndef QV4REGEXP_H
#define QV4REGEXP_H

#include <QString>
#include <QVector>

#include <wtf/RefPtr.h>
#include <wtf/FastAllocBase.h>
#include <wtf/BumpPointerAllocator.h>

#include <limits.h>

#include <yarr/Yarr.h>
#include <yarr/YarrInterpreter.h>

#include "qv4managed.h"
#include "qv4engine.h"

QT_BEGIN_NAMESPACE

namespace QQmlJS {
namespace VM {

struct ExecutionEngine;

struct RegExpCacheKey
{
    RegExpCacheKey(const QString &pattern, bool ignoreCase, bool multiLine)
        : pattern(pattern)
        , ignoreCase(ignoreCase)
        , multiLine(multiLine)
    { }
    explicit inline RegExpCacheKey(const RegExp *re);

    bool operator==(const RegExpCacheKey &other) const
    { return pattern == other.pattern && ignoreCase == other.ignoreCase && multiLine == other.multiLine; }
    bool operator!=(const RegExpCacheKey &other) const
    { return !operator==(other); }

    QString pattern;
    uint ignoreCase : 1;
    uint multiLine : 1;
};

inline uint qHash(const RegExpCacheKey& key, uint seed = 0) Q_DECL_NOTHROW
{ return qHash(key.pattern, seed); }

class RegExpCache : public QHash<RegExpCacheKey, RegExp*>
{
public:
    ~RegExpCache();
};

class RegExp : public Managed
{
public:
    static RegExp* create(ExecutionEngine* engine, const QString& pattern, bool ignoreCase = false, bool multiline = false);
    ~RegExp();

    QString pattern() const { return m_pattern; }

    bool isValid() const { return m_byteCode.get(); }

    uint match(const QString& string, int start, uint *matchOffsets) const;

    bool ignoreCase() const { return m_ignoreCase; }
    bool multiLine() const { return m_multiLine; }
    int captureCount() const { return m_subPatternCount + 1; }

protected:
    static const ManagedVTable static_vtbl;
    static void destroy(Managed *that);
    static void markObjects(Managed *that);
    static Value get(Managed *m, ExecutionContext *ctx, String *name, bool *hasProperty);
    static Value getIndexed(Managed *m, ExecutionContext *ctx, uint index, bool *hasProperty);
    static void put(Managed *m, ExecutionContext *ctx, String *name, const Value &value);
    static void putIndexed(Managed *m, ExecutionContext *ctx, uint index, const Value &value);
    static PropertyAttributes query(Managed *m, ExecutionContext *ctx, String *name);
    static PropertyAttributes queryIndexed(Managed *m, ExecutionContext *ctx, uint index);
    static bool deleteProperty(Managed *m, ExecutionContext *ctx, String *name);
    static bool deleteIndexedProperty(Managed *m, ExecutionContext *ctx, uint index);


private:
    friend class RegExpCache;
    Q_DISABLE_COPY(RegExp);
    RegExp(ExecutionEngine* engine, const QString& pattern, bool ignoreCase, bool multiline);

    const QString m_pattern;
    OwnPtr<JSC::Yarr::BytecodePattern> m_byteCode;
    RegExpCache *m_cache;
    int m_subPatternCount;
    const bool m_ignoreCase;
    const bool m_multiLine;
};

inline RegExpCacheKey::RegExpCacheKey(const RegExp *re)
    : pattern(re->pattern())
    , ignoreCase(re->ignoreCase())
    , multiLine(re->multiLine())
{}


} // end of namespace VM
} // end of namespace QQmlJS

QT_END_NAMESPACE

#endif // QV4REGEXP_H

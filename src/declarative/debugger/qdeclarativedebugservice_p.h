/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QDECLARATIVEDEBUGSERVICE_H
#define QDECLARATIVEDEBUGSERVICE_H

#include <QtCore/qobject.h>

#include <private/qdeclarativeglobal_p.h>

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

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE


class QDeclarativeDebugServicePrivate;
class Q_DECLARATIVE_PRIVATE_EXPORT QDeclarativeDebugService : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QDeclarativeDebugService)
    Q_DISABLE_COPY(QDeclarativeDebugService)

public:
    explicit QDeclarativeDebugService(const QString &, float version, QObject *parent = 0);
    ~QDeclarativeDebugService();

    QString name() const;
    float version() const;

    enum State { NotConnected, Unavailable, Enabled };
    State state() const;

    void sendMessage(const QByteArray &);
    void sendMessages(const QList<QByteArray> &);
    bool waitForMessage();

    static int idForObject(QObject *);
    static QObject *objectForId(int);

    static QString objectToString(QObject *obj);

    static bool isDebuggingEnabled();
    static bool hasDebuggingClient();

protected:
    QDeclarativeDebugService(QDeclarativeDebugServicePrivate &dd, const QString &name, float version, QObject *parent = 0);

    State registerService();

    virtual void stateAboutToBeChanged(State);
    virtual void stateChanged(State);
    virtual void messageReceived(const QByteArray &);

private:
    friend class QDeclarativeDebugServer;
    friend class QDeclarativeDebugServerPrivate;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QDECLARATIVEDEBUGSERVICE_H


/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlthread_p.h"

#include <private/qfieldlist_p.h>

#include <QtCore/qmutex.h>
#include <QtCore/qthread.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qwaitcondition.h>
#include <QtCore/qcoreapplication.h>

QT_BEGIN_NAMESPACE

class QQmlThreadPrivate : public QThread
{
public:
    QQmlThreadPrivate(QQmlThread *);
    QQmlThread *q;

    virtual void run();

    inline void lock() { _mutex.lock(); }
    inline void unlock() { _mutex.unlock(); }
    inline void wait() { _wait.wait(&_mutex); }
    inline void wakeOne() { _wait.wakeOne(); }
    inline void wakeAll() { _wait.wakeAll(); }

    quint32 m_threadProcessing:1; // Set when the thread is processing messages
    quint32 m_mainProcessing:1; // Set when the main thread is processing messages
    quint32 m_shutdown:1; // Set by main thread to request a shutdown
    quint32 m_mainThreadWaiting:1; // Set by main thread if it is waiting for the message queue to empty

    typedef QFieldList<QQmlThread::Message, &QQmlThread::Message::next> MessageList;
    MessageList threadList;
    MessageList mainList;

    QQmlThread::Message *mainSync;

    void triggerMainEvent();
    void triggerThreadEvent();

    void mainEvent();
    void threadEvent();

protected:
    virtual bool event(QEvent *); 

private:
    struct MainObject : public QObject { 
        MainObject(QQmlThreadPrivate *p);
        virtual bool event(QEvent *e);
        QQmlThreadPrivate *p;
    };
    MainObject m_mainObject;

    QMutex _mutex;
    QWaitCondition _wait;
};

QQmlThreadPrivate::MainObject::MainObject(QQmlThreadPrivate *p) 
: p(p) 
{
}

// Trigger mainEvent in main thread.  Must be called from thread.
void QQmlThreadPrivate::triggerMainEvent()
{
    Q_ASSERT(q->isThisThread());
    QCoreApplication::postEvent(&m_mainObject, new QEvent(QEvent::User));
}

// Trigger even in thread.  Must be called from main thread.
void QQmlThreadPrivate::triggerThreadEvent()
{
    Q_ASSERT(!q->isThisThread());
    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

bool QQmlThreadPrivate::MainObject::event(QEvent *e) 
{
    if (e->type() == QEvent::User) 
        p->mainEvent();
    return QObject::event(e);
}
    
QQmlThreadPrivate::QQmlThreadPrivate(QQmlThread *q)
: q(q), m_threadProcessing(false), m_mainProcessing(false), m_shutdown(false), 
  m_mainThreadWaiting(false), mainSync(0), m_mainObject(this)
{
}

bool QQmlThreadPrivate::event(QEvent *e)
{
    if (e->type() == QEvent::User) 
        threadEvent();
    return QThread::event(e);
}

void QQmlThreadPrivate::run()
{
    lock();

    wakeOne();

    unlock();

    q->startupThread();
    exec();
}

void QQmlThreadPrivate::mainEvent()
{
    lock();

    m_mainProcessing = true;

    while (!mainList.isEmpty() || mainSync) {
        bool isSync = mainSync != 0;
        QQmlThread::Message *message = isSync?mainSync:mainList.takeFirst();
        unlock();

        message->call(q);
        delete message;

        lock();

        if (isSync) {
            mainSync = 0;
            wakeOne();
        }
    }

    m_mainProcessing = false;

    unlock();
}

void QQmlThreadPrivate::threadEvent() 
{
    lock();

    if (m_shutdown) {
        quit();
        wakeOne();
        unlock();
        q->shutdownThread();
    } else {
        m_threadProcessing = true;

        while (!threadList.isEmpty()) {
            QQmlThread::Message *message = threadList.first();

            unlock();

            message->call(q);

            lock();

            delete threadList.takeFirst();
        }

        wakeOne();

        m_threadProcessing = false;

        unlock();
    }
}

QQmlThread::QQmlThread()
: d(new QQmlThreadPrivate(this))
{
    d->lock();
    d->start();
    d->wait();
    d->unlock();
    d->moveToThread(d);

}

QQmlThread::~QQmlThread()
{
    delete d;
}

void QQmlThread::shutdown()
{
    d->lock();
    Q_ASSERT(!d->m_shutdown);
    d->m_shutdown = true;
    if (d->threadList.isEmpty() && d->m_threadProcessing == false)
        d->triggerThreadEvent();
    d->wait();
    d->unlock();
    d->QThread::wait();
}

bool QQmlThread::isShutdown() const
{
    return d->m_shutdown;
}

void QQmlThread::lock()
{
    d->lock();
}

void QQmlThread::unlock()
{
    d->unlock();
}

void QQmlThread::wakeOne()
{
    d->wakeOne();
}

void QQmlThread::wakeAll()
{
    d->wakeAll();
}

void QQmlThread::wait()
{
    d->wait();
}

bool QQmlThread::isThisThread() const
{
    return QThread::currentThread() == d;
}

QThread *QQmlThread::thread() const
{
    return const_cast<QThread *>(static_cast<const QThread *>(d));
}

// Called when the thread starts.  Do startup stuff in here.
void QQmlThread::startupThread()
{
}

// Called when the thread shuts down.  Do cleanup in here.
void QQmlThread::shutdownThread()
{
}

void QQmlThread::internalCallMethodInThread(Message *message)
{
    Q_ASSERT(!isThisThread());
    d->lock();
    Q_ASSERT(d->m_mainThreadWaiting == false);

    bool wasEmpty = d->threadList.isEmpty();
    d->threadList.append(message);
    if (wasEmpty && d->m_threadProcessing == false)
        d->triggerThreadEvent();

    d->m_mainThreadWaiting = true;

    do {
        if (d->mainSync) {
            QQmlThread::Message *message = d->mainSync;
            unlock();
            message->call(this);
            delete message;
            lock();
            d->mainSync = 0;
            wakeOne();
        } else {
            d->wait();
        }
    } while (d->mainSync || !d->threadList.isEmpty());

    d->m_mainThreadWaiting = false;
    d->unlock();
}

void QQmlThread::internalCallMethodInMain(Message *message)
{
    Q_ASSERT(isThisThread());

    d->lock();

    Q_ASSERT(d->mainSync == 0);
    d->mainSync = message;

    if (d->m_mainThreadWaiting) {
        d->wakeOne();
    } else if (d->m_mainProcessing) {
        // Do nothing - it is already looping
    } else {
        d->triggerMainEvent();
    }

    while (d->mainSync && !d->m_shutdown)
        d->wait();

    d->unlock();
}

void QQmlThread::internalPostMethodToThread(Message *message)
{
    Q_ASSERT(!isThisThread());
    d->lock();
    bool wasEmpty = d->threadList.isEmpty();
    d->threadList.append(message);
    if (wasEmpty && d->m_threadProcessing == false)
        d->triggerThreadEvent();
    d->unlock();
}

void QQmlThread::internalPostMethodToMain(Message *message)
{
    Q_ASSERT(isThisThread());
    d->lock();
    bool wasEmpty = d->mainList.isEmpty();
    d->mainList.append(message);
    if (wasEmpty && d->m_mainProcessing == false)
        d->triggerMainEvent();
    d->unlock();
}

QT_END_NAMESPACE

// Commit: 47712d1f330e4b22ce6dd30e7557288ef7f7fca0
/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSGTEXTINPUT_P_P_H
#define QSGTEXTINPUT_P_P_H

#include "qsgtextinput_p.h"
#include "qsgtext_p.h"
#include "qsgimplicitsizeitem_p_p.h"

#include <private/qlinecontrol_p.h>

#include <QtDeclarative/qdeclarative.h>
#include <QtCore/qpointer.h>


//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

QT_BEGIN_NAMESPACE

class QSGTextNode;

class Q_AUTOTEST_EXPORT QSGTextInputPrivate : public QSGImplicitSizeItemPrivate
{
    Q_DECLARE_PUBLIC(QSGTextInput)
public:
    QSGTextInputPrivate()
                 : control(new QLineControl(QString()))
                 , color((QRgb)0)
                 , style(QSGText::Normal)
                 , styleColor((QRgb)0)
                 , hAlign(QSGTextInput::AlignLeft)
                 , mouseSelectionMode(QSGTextInput::SelectCharacters)
                 , inputMethodHints(Qt::ImhNone)
                 , textNode(0)
                 , hscroll(0)
                 , oldScroll(0)
                 , oldValidity(false)
                 , focused(false)
                 , focusOnPress(true)
                 , showInputPanelOnFocus(true)
                 , clickCausedFocus(false)
                 , cursorVisible(false)
                 , autoScroll(true)
                 , selectByMouse(false)
                 , canPaste(false)
                 , hAlignImplicit(true)
                 , selectPressed(false)
                 , textLayoutDirty(true)
    {
#ifdef Q_OS_SYMBIAN
        if (QSysInfo::symbianVersion() == QSysInfo::SV_SF_1 || QSysInfo::symbianVersion() == QSysInfo::SV_SF_3) {
            showInputPanelOnFocus = false;
        }
#endif
    }

    ~QSGTextInputPrivate()
    {
    }

    int xToPos(int x, QTextLine::CursorPosition betweenOrOn = QTextLine::CursorBetweenCharacters) const
    {
        Q_Q(const QSGTextInput);
        QRect cr = q->boundingRect().toRect();
        x-= cr.x() - hscroll;
        return control->xToPos(x, betweenOrOn);
    }

    void init();
    void startCreatingCursor();
    void updateHorizontalScroll();
    bool determineHorizontalAlignment();
    bool setHAlign(QSGTextInput::HAlignment, bool forceAlign = false);
    void mirrorChange();
    int calculateTextWidth();
    bool sendMouseEventToInputContext(QGraphicsSceneMouseEvent *event, QEvent::Type eventType);
    void updateInputMethodHints();
    void hideCursor();
    void showCursor();

    QLineControl* control;

    QFont font;
    QFont sourceFont;
    QColor  color;
    QColor  selectionColor;
    QColor  selectedTextColor;
    QSGText::TextStyle style;
    QColor  styleColor;
    QSGTextInput::HAlignment hAlign;
    QSGTextInput::SelectionMode mouseSelectionMode;
    Qt::InputMethodHints inputMethodHints;
    QPointer<QDeclarativeComponent> cursorComponent;
    QPointer<QSGItem> cursorItem;
    QPointF pressPos;
    QSGTextNode *textNode;

    int lastSelectionStart;
    int lastSelectionEnd;
    int oldHeight;
    int oldWidth;
    int hscroll;
    int oldScroll;

    bool oldValidity:1;
    bool focused:1;
    bool focusOnPress:1;
    bool showInputPanelOnFocus:1;
    bool clickCausedFocus:1;
    bool cursorVisible:1;
    bool autoScroll:1;
    bool selectByMouse:1;
    bool canPaste:1;
    bool hAlignImplicit:1;
    bool selectPressed:1;
    bool textLayoutDirty:1;

    static inline QSGTextInputPrivate *get(QSGTextInput *t) {
        return t->d_func();
    }
};

QT_END_NAMESPACE

#endif // QSGTEXTINPUT_P_P_H

/* This file is part of the KDE project
   Copyright (C) 2002 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
   Copyright (C) 2002 John Firebaugh <jfirebaugh@kde.org>
   Copyright (C) 2007 Hamish Rodda <rodda@kde.org>
   Copyright (C) 2009 Niko Sams <niko.sams@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/
#ifndef BREAKPOINTCONTROLLER_H
#define BREAKPOINTCONTROLLER_H

#include <QObject>

#include <debugger/interfaces/ibreakpointcontroller.h>

#include "gdbglobal.h"

class QModelIndex;

namespace GDBMI {
class ResultRecord;
class Value;
}

namespace GDBDebugger
{
class DebugSession;
class GDBController;
struct InsertedHandler;
struct EnabledOrDisabledHandler;
struct DeletedHandler;
/**
* Handles signals from the editor that relate to breakpoints and the execution
* point of the debugger.
* We may change, add or remove breakpoints in this class.
*/
class BreakpointController : public KDevelop::IBreakpointController
{
    Q_OBJECT

public:
    BreakpointController(DebugSession* parent);

private slots:
    void slotEvent(event_t);

private:
    GDBController* controller() const;
    DebugSession* debugSession() const;

    virtual void sendMaybe(KDevelop::Breakpoint *breakpoint);

    void handleInserted(const GDBMI::ResultRecord &r);
    void handleBreakpointList(const GDBMI::ResultRecord &r);

    void update(KDevelop::Breakpoint *b, const GDBMI::Value& v);

    friend struct InsertedHandler;
    friend struct EnabledOrDisabledHandler;
    friend struct DeletedHandler;

};

}

#endif

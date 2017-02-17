/*
 * LLDB Debugger Support
 * Copyright 2016  Aetf <aetf@unlimitedcodeworks.xyz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef LLDB_DEBUGGERPLUGIN_H
#define LLDB_DEBUGGERPLUGIN_H

#include "midebuggerplugin.h"

#include "debugsession.h"
#include "widgets/debuggerconsoleview.h"
#include "widgets/disassemblewidget.h"

namespace KDevMI { namespace LLDB {

class LldbLauncher;

class NonInterruptDebuggerConsoleView : public DebuggerConsoleView
{
public:
    explicit NonInterruptDebuggerConsoleView(MIDebuggerPlugin *plugin, QWidget *parent = nullptr)
        : DebuggerConsoleView(plugin, parent)
    {
        setShowInterrupt(false);
        setReplacePrompt("(lldb)");
    }
};

class LldbDebuggerPlugin : public MIDebuggerPlugin
{
    Q_OBJECT

public:
    friend class KDevMI::LLDB::DebugSession;

    explicit LldbDebuggerPlugin(QObject *parent, const QVariantList & = QVariantList());
    ~LldbDebuggerPlugin() override;

    void unload() override;

    DebugSession *createSession() override;
    void unloadToolviews() override;
    void setupToolviews() override;

private:
    void setupExecutePlugin(KDevelop::IPlugin* plugin, bool load);

    DebuggerToolFactory<NonInterruptDebuggerConsoleView> *m_consoleFactory;
    DebuggerToolFactory<DisassembleWidget> *m_disassembleFactory;
    QHash<KDevelop::IPlugin*, LldbLauncher*> m_launchers;
};

} // end of namespace LLDB
} // end of namespace KDevMI

#endif // LLDB_DEBUGGERPLUGIN_H

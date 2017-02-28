/*
 * Low level GDB interface.
 *
 * Copyright 1999 John Birch <jbb@kdevelop.org >
 * Copyright 2007 Vladimir Prus <ghost@cs.msu.su>
 * Copyright 2016 Aetf <aetf@unlimitedcodeworks.xyz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "midebugger.h"

#include "debuglog.h"
#include "mi/micommand.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QApplication>
#include <QString>
#include <QStringList>

#include <signal.h>

#include <memory>
#include <stdexcept>
#include <sstream>

// #define DEBUG_NO_TRY //to get a backtrace to where the exception was thrown

using namespace KDevMI;
using namespace KDevMI::MI;

MIDebugger::MIDebugger(QObject* parent)
    : QObject(parent)
    , process_(nullptr)
    , currentCmd_(nullptr)
{
    process_ = new KProcess(this);
    process_->setOutputChannelMode(KProcess::SeparateChannels);
    connect(process_, &KProcess::readyReadStandardOutput,
            this, &MIDebugger::readyReadStandardOutput);
    connect(process_, &KProcess::readyReadStandardError,
            this, &MIDebugger::readyReadStandardError);
    connect(process_,
            static_cast<void(KProcess::*)(int,QProcess::ExitStatus)>(&KProcess::finished),
            this, &MIDebugger::processFinished);
    connect(process_, static_cast<void(KProcess::*)(QProcess::ProcessError)>(&KProcess::error),
            this, &MIDebugger::processErrored);
}

MIDebugger::~MIDebugger()
{
    // prevent Qt warning: QProcess: Destroyed while process is still running.
    if (process_ && process_->state() == QProcess::Running) {
        disconnect(process_, static_cast<void(KProcess::*)(QProcess::ProcessError)>(&KProcess::error),
                    this, &MIDebugger::processErrored);
        process_->kill();
        process_->waitForFinished(10);
    }
}

void MIDebugger::execute(MICommand* command)
{
    currentCmd_ = command;
    QString commandText = currentCmd_->cmdToSend();

    qCDebug(DEBUGGERCOMMON) << "SEND:" << commandText.trimmed();

    QByteArray commandUtf8 = commandText.toUtf8();

    process_->write(commandUtf8, commandUtf8.length());
    command->markAsSubmitted();

    QString prettyCmd = currentCmd_->cmdToSend();
    prettyCmd.remove( QRegExp("set prompt \032.\n") );
    prettyCmd = "(gdb) " + prettyCmd;

    if (currentCmd_->isUserCommand())
        emit userCommandOutput(prettyCmd);
    else
        emit internalCommandOutput(prettyCmd);
}

bool MIDebugger::isReady() const
{
    return currentCmd_ == nullptr;
}

void MIDebugger::interrupt()
{
    //TODO:win32 Porting needed
    int pid = process_->pid();
    if (pid != 0) {
        ::kill(pid, SIGINT);
    }
}

MICommand* MIDebugger::currentCommand() const
{
    return currentCmd_;
}

void MIDebugger::kill()
{
    process_->kill();
}

void MIDebugger::readyReadStandardOutput()
{
    process_->setReadChannel(QProcess::StandardOutput);

    buffer_ += process_->readAll();
    for (;;)
    {
        /* In MI mode, all messages are exactly one line.
           See if we have any complete lines in the buffer. */
        int i = buffer_.indexOf('\n');
        if (i == -1)
            break;
        QByteArray reply(buffer_.left(i));
        buffer_ = buffer_.mid(i+1);

        processLine(reply);
    }
}

void MIDebugger::readyReadStandardError()
{
    process_->setReadChannel(QProcess::StandardError);
    emit debuggerInternalOutput(QString::fromUtf8(process_->readAll()));
}

void MIDebugger::processLine(const QByteArray& line)
{
    qCDebug(DEBUGGERCOMMON) << "Debugger (" << process_->pid() <<") output: " << line;

    FileSymbol file;
    file.contents = line;

    std::unique_ptr<MI::Record> r(mi_parser_.parse(&file));

    if (!r)
    {
        // simply ignore the invalid MI message because both gdb and lldb
        // sometimes produces invalid messages that can be safely ignored.
        qCDebug(DEBUGGERCOMMON) << "Invalid MI message:" << line;
        // We don't consider the current command done.
        // So, if a command results in unparseable reply,
        // we'll just wait for the "right" reply, which might
        // never come.  However, marking the command as
        // done in this case is even more risky.
        // It's probably possible to get here if we're debugging
        // natively without PTY, though this is uncommon case.
        return;
    }

    #ifndef DEBUG_NO_TRY
    try
    {
    #endif
        switch(r->kind)
        {
        case MI::Record::Result: {
            MI::ResultRecord& result = static_cast<MI::ResultRecord&>(*r);

            // it's still possible for the user to issue a MI command,
            // emit correct signal
            if (currentCmd_ && currentCmd_->isUserCommand()) {
                emit userCommandOutput(QString::fromUtf8(line) + '\n');
            } else {
                emit internalCommandOutput(QString::fromUtf8(line) + '\n');
            }

            // protect against wild replies that sometimes returned from gdb without a pending command
            if (!currentCmd_)
            {
                qCWarning(DEBUGGERCOMMON) << "Received a result without a pending command";
                throw std::runtime_error("Received a result without a pending command");
            }
            else if (currentCmd_->token() != result.token)
            {
                std::stringstream ss;
                ss << "Received a result with token not matching pending command. "
                   << "Pending: " << currentCmd_->token() << "Received: " << result.token;
                qCWarning(DEBUGGERCOMMON) << ss.str().c_str();
                throw std::runtime_error(ss.str());
            }

            // GDB doc: "running" and "exit" are status codes equivalent to "done"
            if (result.reason == "done" || result.reason == "running" || result.reason == "exit")
            {
                qCDebug(DEBUGGERCOMMON) << "Result token is" << result.token;
                currentCmd_->markAsCompleted();
                qCDebug(DEBUGGERCOMMON) << "Command successful, times "
                                        << currentCmd_->totalProcessingTime()
                                        << currentCmd_->queueTime()
                                        << currentCmd_->gdbProcessingTime();
                currentCmd_->invokeHandler(result);
            }
            else if (result.reason == "error")
            {
                qCDebug(DEBUGGERCOMMON) << "Handling error";
                currentCmd_->markAsCompleted();
                qCDebug(DEBUGGERCOMMON) << "Command error, times"
                                        << currentCmd_->totalProcessingTime()
                                        << currentCmd_->queueTime()
                                        << currentCmd_->gdbProcessingTime();
                // Some commands want to handle errors themself.
                if (currentCmd_->handlesError() &&
                    currentCmd_->invokeHandler(result))
                {
                    qCDebug(DEBUGGERCOMMON) << "Invoked custom handler\n";
                    // Done, nothing more needed
                }
                else
                    emit error(result);
            }
            else
            {
                qCDebug(DEBUGGERCOMMON) << "Unhandled result code: " << result.reason;
            }

            delete currentCmd_;
            currentCmd_ = nullptr;
            emit ready();
            break;
        }

        case MI::Record::Async: {
            MI::AsyncRecord& async = dynamic_cast<MI::AsyncRecord&>(*r);

            switch (async.subkind) {
            case MI::AsyncRecord::Exec: {
                // Prefix '*'; asynchronous state changes of the target
                if (async.reason == "stopped")
                {
                    emit programStopped(async);
                }
                else if (async.reason == "running")
                {
                    emit programRunning();
                }
                else
                {
                    qCDebug(DEBUGGERCOMMON) << "Unhandled exec notification: " << async.reason;
                }
                break;
            }

            case MI::AsyncRecord::Notify: {
                // Prefix '='; supplementary information that we should handle (new breakpoint etc.)
                emit notification(async);
                break;
            }

            case MI::AsyncRecord::Status: {
                // Prefix '+'; GDB documentation:
                // On-going status information about progress of a slow operation; may be ignored
                break;
            }

            default:
                Q_ASSERT(false);
            }
            break;
        }

        case MI::Record::Stream: {

            MI::StreamRecord& s = dynamic_cast<MI::StreamRecord&>(*r);

            if (s.subkind == MI::StreamRecord::Target) {
                emit applicationOutput(s.message);
            } else if (s.subkind == MI::StreamRecord::Console) {
                if (currentCmd_ && currentCmd_->isUserCommand())
                    emit userCommandOutput(s.message);
                else
                    emit internalCommandOutput(s.message);

                if (currentCmd_)
                    currentCmd_->newOutput(s.message);
            } else {
                emit debuggerInternalOutput(s.message);
            }

            emit streamRecord(s);

            break;
        }

        case MI::Record::Prompt:
            break;
        }
    #ifndef DEBUG_NO_TRY
    }
    catch(const std::exception& e)
    {
        KMessageBox::detailedSorry(
            qApp->activeWindow(),
            i18nc("<b>Internal debugger error</b>",
                    "<p>The debugger component encountered internal error while "
                    "processing reply from gdb. Please submit a bug report. "
                    "The debug session will now end to prevent potential crash"),
            i18n("The exception is: %1\n"
                "The MI response is: %2", e.what(),
                QString::fromLatin1(line)),
            i18n("Internal debugger error"));
        emit exited(true, e.what());
    }
    #endif
}

void MIDebugger::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qCDebug(DEBUGGERCOMMON) << "Debugger FINISHED\n";

    bool abnormal = exitCode != 0 || exitStatus != QProcess::NormalExit;
    emit userCommandOutput("Process exited\n");
    emit exited(abnormal, i18n("Process exited"));
}

void MIDebugger::processErrored(QProcess::ProcessError error)
{
    qCDebug(DEBUGGERCOMMON) << "Debugger ERRORED" << error;
    if(error == QProcess::FailedToStart)
    {
        KMessageBox::information(
            qApp->activeWindow(),
            i18n("<b>Could not start debugger.</b>"
                 "<p>Could not run '%1'. "
                 "Make sure that the path name is specified correctly.",
                 debuggerExecutable_),
            i18n("Could not start debugger"));

        emit userCommandOutput("Process failed to start\n");
        emit exited(true, i18n("Process failed to start"));

    } else if (error == QProcess::Crashed) {
        KMessageBox::error(
            qApp->activeWindow(),
            i18n("<b>Debugger crashed.</b>"
                 "<p>The debugger process '%1' crashed.<br>"
                 "Because of that the debug session has to be ended.<br>"
                 "Try to reproduce the crash without KDevelop and report a bug.<br>",
                 debuggerExecutable_),
            i18n("Debugger crashed"));

        emit userCommandOutput("Process crashed\n");
        emit exited(true, i18n("Process crashed"));
    }
}

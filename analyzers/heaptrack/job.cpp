/* This file is part of KDevelop
   Copyright 2017 Anton Anikin <anton.anikin@htower.ru>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "job.h"

#include "debug.h"
#include "globalsettings.h"
#include "utils.h"

#include <execute/iexecuteplugin.h>
#include <interfaces/icore.h>
#include <interfaces/iplugincontroller.h>
#include <interfaces/iruncontroller.h>
#include <interfaces/iuicontroller.h>
#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <util/environmentprofilelist.h>
#include <util/path.h>

#include <QFileInfo>
#include <QRegularExpression>

namespace Heaptrack
{

Job::Job(KDevelop::ILaunchConfiguration* launchConfig)
    : m_pid(-1)
{
    Q_ASSERT(launchConfig);

    auto pluginController = KDevelop::ICore::self()->pluginController();
    auto iface = pluginController->pluginForExtension(QStringLiteral("org.kdevelop.IExecutePlugin"))->extension<IExecutePlugin>();
    Q_ASSERT(iface);

    QString envProfile = iface->environmentProfileName(launchConfig);
    if (envProfile.isEmpty()) {
        envProfile = KDevelop::EnvironmentProfileList(KSharedConfig::openConfig()).defaultProfileName();
    }
    setEnvironmentProfile(envProfile);

    QString errorString;

    m_analyzedExecutable = iface->executable(launchConfig, errorString).toLocalFile();
    if (!errorString.isEmpty()) {
        setError(-1);
        setErrorText(errorString);
    }

    QStringList analyzedExecutableArguments = iface->arguments(launchConfig, errorString);
    if (!errorString.isEmpty()) {
        setError(-1);
        setErrorText(errorString);
    }

    QUrl workDir = iface->workingDirectory(launchConfig);
    if (workDir.isEmpty() || !workDir.isValid()) {
        workDir = QUrl::fromLocalFile(QFileInfo(m_analyzedExecutable).absolutePath());
    }
    setWorkingDirectory(workDir);

    *this << KDevelop::Path(GlobalSettings::heaptrackExecutable()).toLocalFile();
    *this << m_analyzedExecutable;
    *this << analyzedExecutableArguments;

    setup();
}

Job::Job(long int pid)
    : m_pid(pid)
{
    *this << KDevelop::Path(GlobalSettings::heaptrackExecutable()).toLocalFile();
    *this << QStringLiteral("-p");
    *this << QString::number(m_pid);

    setup();
}

void Job::setup()
{
    setProperties(DisplayStdout);
    setProperties(DisplayStderr);
    setProperties(PostProcessOutput);

    setCapabilities(Killable);
    setStandardToolView(KDevelop::IOutputView::TestView);
    setBehaviours(KDevelop::IOutputView::AutoScroll);

    KDevelop::ICore::self()->uiController()->registerStatus(this);
    connect(this, &Job::finished, this, [this]() {
        emit hideProgress(this);
    });
}

Job::~Job()
{
}

QString Job::statusName() const
{
    QString target = m_pid < 0 ? QFileInfo(m_analyzedExecutable).fileName()
                               : QString("PID: %1").arg(m_pid);
    return i18n("Heaptrack Analysis (%1)", target);
}

QString Job::resultsFile() const
{
    return m_resultsFile;
}

void Job::start()
{
    emit showProgress(this, 0, 0, 0);
    OutputExecuteJob::start();
}

void Job::postProcessStdout(const QStringList& lines)
{
    static const auto resultRegex =
        QRegularExpression(QStringLiteral("heaptrack output will be written to \\\"(.+)\\\""));

    if (m_resultsFile.isEmpty()) {
        QRegularExpressionMatch match;
        for (const QString & line : lines) {
            match = resultRegex.match(line);
            if (match.hasMatch()) {
                m_resultsFile = match.captured(1);
                break;
            }
        }
    }

    OutputExecuteJob::postProcessStdout(lines);
}

}

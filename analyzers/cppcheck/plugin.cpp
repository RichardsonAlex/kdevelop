/* This file is part of KDevelop
   Copyright 2013 Christoph Thielecke <crissi99@gmx.de>
   Copyright 2016-2017 Anton Anikin <anton.anikin@htower.ru>

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

#include "plugin.h"

#include "config/globalconfigpage.h"
#include "config/projectconfigpage.h"
#include "globalsettings.h"

#include "debug.h"
#include "job.h"
#include "problemmodel.h"

#include <interfaces/contextmenuextension.h>
#include <interfaces/icore.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iruncontroller.h>
#include <interfaces/iuicontroller.h>
#include <kactioncollection.h>
#include <kpluginfactory.h>
#include <language/interfaces/editorcontext.h>
#include <project/projectconfigpage.h>
#include <project/projectmodel.h>
#include <util/jobstatus.h>

#include <QAction>
#include <QMessageBox>

K_PLUGIN_FACTORY_WITH_JSON(CppcheckFactory, "kdevcppcheck.json", registerPlugin<cppcheck::Plugin>();)

namespace cppcheck
{

Plugin::Plugin(QObject* parent, const QVariantList&)
    : IPlugin("kdevcppcheck", parent)
    , m_job(nullptr)
    , m_currentProject(nullptr)
    , m_model(new ProblemModel(this))
{
    qCDebug(KDEV_CPPCHECK) << "setting cppcheck rc file";
    setXMLFile("kdevcppcheck.rc");

    m_actionFile = new QAction(QIcon::fromTheme("cppcheck"), i18n("Cppcheck (Current File)"), this);
    connect(m_actionFile, &QAction::triggered, [this](){
        runCppcheck(false);
    });
    actionCollection()->addAction("cppcheck_file", m_actionFile);

    m_actionProject = new QAction(QIcon::fromTheme("cppcheck"), i18n("Cppcheck (Current Project)"), this);
    connect(m_actionProject, &QAction::triggered, [this](){
        runCppcheck(true);
    });
    actionCollection()->addAction("cppcheck_project", m_actionProject);

    m_actionProjectItem = new QAction(QIcon::fromTheme("cppcheck"), i18n("Cppcheck"), this);

    connect(core()->documentController(), &KDevelop::IDocumentController::documentClosed,
            this, &Plugin::updateActions);
    connect(core()->documentController(), &KDevelop::IDocumentController::documentActivated,
            this, &Plugin::updateActions);

    connect(core()->projectController(), &KDevelop::IProjectController::projectOpened,
            this, &Plugin::updateActions);
    connect(core()->projectController(), &KDevelop::IProjectController::projectClosed,
            this, &Plugin::projectClosed);

    updateActions();
}

Plugin::~Plugin()
{
    killCppcheck();
}

bool Plugin::isRunning()
{
    return m_job;
}

void Plugin::killCppcheck()
{
    if (m_job) {
        m_job->kill(KJob::EmitResult);
    }
}

void Plugin::raiseProblemsView()
{
    m_model->show();
}

void Plugin::raiseOutputView()
{
    core()->uiController()->findToolView(
        i18nc("@title:window", "Test"),
        nullptr,
        KDevelop::IUiController::FindFlags::Raise);
}

void Plugin::updateActions()
{
    m_currentProject = nullptr;

    m_actionFile->setEnabled(false);
    m_actionProject->setEnabled(false);

    if (isRunning()) {
        return;
    }

    KDevelop::IDocument* activeDocument = core()->documentController()->activeDocument();
    if (!activeDocument) {
        return;
    }

    QUrl url = activeDocument->url();

    m_currentProject = core()->projectController()->findProjectForUrl(url);
    if (!m_currentProject) {
        return;
    }

    m_actionFile->setEnabled(true);
    m_actionProject->setEnabled(true);
}

void Plugin::projectClosed(KDevelop::IProject* project)
{
    if (project != m_model->project()) {
        return;
    }

    killCppcheck();
    m_model->reset();
}

void Plugin::runCppcheck(bool checkProject)
{
    KDevelop::IDocument* doc = core()->documentController()->activeDocument();
    Q_ASSERT(doc);

    if (checkProject) {
        runCppcheck(m_currentProject, m_currentProject->path().toUrl().toLocalFile());
    } else {
        runCppcheck(m_currentProject, doc->url().toLocalFile());
    }
}

void Plugin::runCppcheck(KDevelop::IProject* project, const QString& path)
{
    m_model->reset(project, path);

    Parameters params(project);
    params.checkPath = path;

    m_job = new Job(params);

    connect(m_job, &Job::problemsDetected, m_model.data(), &ProblemModel::addProblems);
    connect(m_job, &Job::finished, this, &Plugin::result);

    core()->uiController()->registerStatus(new KDevelop::JobStatus(m_job, "Cppcheck"));
    core()->runController()->registerJob(m_job);

    if (params.hideOutputView) {
        raiseProblemsView();
    } else {
        raiseOutputView();
    }

    updateActions();
}

void Plugin::result(KJob*)
{
    if (!core()->projectController()->projects().contains(m_model->project())) {
        m_model->reset();
    } else {
        m_model->setProblems();

        if (m_job->status() == KDevelop::OutputExecuteJob::JobStatus::JobSucceeded ||
            m_job->status() == KDevelop::OutputExecuteJob::JobStatus::JobCanceled) {
            raiseProblemsView();
        } else {
            raiseOutputView();
        }
    }

    m_job = nullptr; // job is automatically deleted later

    updateActions();
}

KDevelop::ContextMenuExtension Plugin::contextMenuExtension(KDevelop::Context* context)
{
    KDevelop::ContextMenuExtension extension = KDevelop::IPlugin::contextMenuExtension(context);

    if (context->hasType(KDevelop::Context::EditorContext) &&
        m_currentProject && !isRunning()) {

        extension.addAction(KDevelop::ContextMenuExtension::AnalyzeGroup, m_actionFile);
        extension.addAction(KDevelop::ContextMenuExtension::AnalyzeGroup, m_actionProject);
    }

    if (context->hasType(KDevelop::Context::ProjectItemContext) && !isRunning()) {
        auto pContext = dynamic_cast<KDevelop::ProjectItemContext*>(context);
        if (pContext->items().size() != 1) {
            return extension;
        }

        auto item = pContext->items().first();

        switch (item->type()) {
            case KDevelop::ProjectBaseItem::File:
            case KDevelop::ProjectBaseItem::Folder:
            case KDevelop::ProjectBaseItem::BuildFolder:
                break;

            default:
                return extension;
        }

        m_actionProjectItem->disconnect();
        connect(m_actionProjectItem, &QAction::triggered, [this, item](){
            runCppcheck(item->project(), item->path().toLocalFile());
        });

        extension.addAction(KDevelop::ContextMenuExtension::AnalyzeGroup, m_actionProjectItem);
    }

    return extension;
}

KDevelop::ConfigPage* Plugin::perProjectConfigPage(int number, const KDevelop::ProjectConfigOptions& options, QWidget* parent)
{
    return number ? nullptr : new ProjectConfigPage(this, options.project, parent);
}

KDevelop::ConfigPage* Plugin::configPage(int number, QWidget* parent)
{
    return number ? nullptr : new GlobalConfigPage(this, parent);
}

}

#include "plugin.moc"

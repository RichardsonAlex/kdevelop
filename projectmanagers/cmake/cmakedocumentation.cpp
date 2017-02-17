/* KDevelop CMake Support
 *
 * Copyright 2009 Aleix Pol <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "cmakedocumentation.h"
#include "cmakeutils.h"

#include <QProcess>
#include <QString>
#include <QTextDocument>
#include <QStringListModel>
#include <QMimeDatabase>
#include <QFontDatabase>
#include <QStandardPaths>

#include <interfaces/iproject.h>
#include <KPluginFactory>
#include <documentation/standarddocumentationview.h>
#include <language/duchain/declaration.h>
#include <interfaces/iplugincontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/icore.h>
#include "cmakemanager.h"
#include "cmakeparserutils.h"
#include "cmakehelpdocumentation.h"
#include "cmakedoc.h"
#include "debug.h"

K_PLUGIN_FACTORY_WITH_JSON(CMakeSupportDocFactory, "kdevcmakedocumentation.json", registerPlugin<CMakeDocumentation>(); )

CMakeDocumentation* CMakeDoc::s_provider=nullptr;
KDevelop::IDocumentationProvider* CMakeDoc::provider() const { return s_provider; }

CMakeDocumentation::CMakeDocumentation(QObject* parent, const QVariantList&)
    : KDevelop::IPlugin( "kdevcmakedocumentation", parent )
    , m_cmakeExecutable(CMake::findExecutable())
    , m_index(nullptr)
{
    if (m_cmakeExecutable.isEmpty()) {
        setErrorDescription(i18n("Unable to find a CMake executable. Is one installed on the system?"));
        return;
    }

    CMakeDoc::s_provider=this;
    m_index= new QStringListModel(this);
    initializeModel();
}

static const char* const args[] = { "--help-command", "--help-variable", "--help-module", "--help-property", nullptr, nullptr };

void CMakeDocumentation::delayedInitialization()
{
    for(int i=0; i<=Property; i++) {
        collectIds(QString(args[i])+"-list", (Type) i);
    }

    m_index->setStringList(m_typeForName.keys());
}

void CMakeDocumentation::collectIds(const QString& param, Type type)
{
    QStringList ids = CMake::executeProcess(m_cmakeExecutable, QStringList(param)).split(QLatin1Char('\n'));
    ids.takeFirst();
    foreach(const QString& name, ids)
    {
        m_typeForName[name]=type;
    }
}

QStringList CMakeDocumentation::names(CMakeDocumentation::Type t) const
{
    return m_typeForName.keys(t);
}

QString CMakeDocumentation::descriptionForIdentifier(const QString& id, Type t) const
{
    QString desc;
    if(args[t]) {
        desc = CMake::executeProcess(m_cmakeExecutable, { args[t], id.simplified() });
        desc = desc.remove(":ref:");

        const QString rst2html = QStandardPaths::findExecutable(QStringLiteral("rst2html"));
        if (rst2html.isEmpty()) {
            desc = ("<html><body style='background:#fff'><pre><code>" + desc.toHtmlEscaped() + "</code></pre>"
                + i18n("<p>For better cmake documentation rendering, install rst2html</p>")
                + "</body></html>");
        } else {
            QProcess p;
            p.start(rst2html, { "--no-toc-backlinks" });
            p.write(desc.toUtf8());
            p.closeWriteChannel();
            p.waitForFinished();
            desc = QString::fromUtf8(p.readAllStandardOutput());
        }
    }

    return desc;
}

KDevelop::IDocumentation::Ptr CMakeDocumentation::description(const QString& identifier, const QUrl &file) const
{
    initializeModel(); //make it not queued
    if (!file.isEmpty() && !QMimeDatabase().mimeTypeForUrl(file).inherits("text/x-cmake")) {
        return KDevelop::IDocumentation::Ptr();
    }

    QString desc;

    if(m_typeForName.contains(identifier)) {
        desc=descriptionForIdentifier(identifier, m_typeForName[identifier]);
    } else if(m_typeForName.contains(identifier.toLower())) {
        desc=descriptionForIdentifier(identifier, m_typeForName[identifier.toLower()]);
    } else if(m_typeForName.contains(identifier.toUpper())) {
        desc=descriptionForIdentifier(identifier, m_typeForName[identifier.toUpper()]);
    }

    KDevelop::IProject* p=KDevelop::ICore::self()->projectController()->findProjectForUrl(file);
    ICMakeManager* m=nullptr;
    if(p)
        m=p->managerPlugin()->extension<ICMakeManager>();
    if(m)
    {
        QPair<QString, QString> entry = m->cacheValue(p, identifier);
        if(!entry.first.isEmpty())
            desc += i18n("<br /><em>Cache Value:</em> %1\n", entry.first);

        if(!entry.second.isEmpty())
            desc += i18n("<br /><em>Cache Documentation:</em> %1\n", entry.second);
    }

    if(desc.isEmpty())
        return KDevelop::IDocumentation::Ptr();
    else
        return KDevelop::IDocumentation::Ptr(new CMakeDoc(identifier, desc));
}

KDevelop::IDocumentation::Ptr CMakeDocumentation::documentationForDeclaration(KDevelop::Declaration* decl) const
{
    return description(decl->identifier().toString(), decl->url().toUrl());
}

KDevelop::IDocumentation::Ptr CMakeDocumentation::documentationForIndex(const QModelIndex& idx) const
{
    return description(idx.data().toString(), QUrl());
}

QAbstractListModel* CMakeDocumentation::indexModel() const
{
    initializeModel();
    return m_index;
}

QIcon CMakeDocumentation::icon() const
{
    return QIcon::fromTheme("cmake");
}

QString CMakeDocumentation::name() const
{
    return "CMake";
}

KDevelop::IDocumentation::Ptr CMakeDocumentation::homePage() const
{
    if(m_typeForName.isEmpty())
        const_cast<CMakeDocumentation*>(this)->delayedInitialization();
//     initializeModel();
    return KDevelop::IDocumentation::Ptr(new CMakeHomeDocumentation);
}

void CMakeDocumentation::initializeModel() const
{
    if(!m_typeForName.isEmpty())
        return;

    QMetaObject::invokeMethod(const_cast<CMakeDocumentation*>(this), "delayedInitialization", Qt::QueuedConnection);
}

//////////CMakeDoc

QWidget* CMakeDoc::documentationWidget(KDevelop::DocumentationFindWidget* findWidget, QWidget* parent)
{
    KDevelop::StandardDocumentationView* view = new KDevelop::StandardDocumentationView(findWidget, parent);
    view->setHtml(mDesc);
    return view;
}

#include "cmakedocumentation.moc"

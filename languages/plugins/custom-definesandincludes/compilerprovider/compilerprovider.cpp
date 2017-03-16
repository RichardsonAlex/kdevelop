/*
 * This file is part of KDevelop
 *
 * Copyright 2014 Sergey Kalinichev <kalinichev.so.0@gmail.com>
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

#include "compilerprovider.h"

#include "../debugarea.h"

#include "compilerfactories.h"
#include "settingsmanager.h"

#include <interfaces/icore.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <project/projectmodel.h>

#include <KPluginFactory>
#include <KAboutData>
#include <KLocalizedString>
#include <QStandardPaths>

using namespace KDevelop;

namespace
{
class NoCompiler : public ICompiler
{
public:
    NoCompiler():
        ICompiler(i18n("None"), QString(), QString(), QString(), false)
    {}

    QHash< QString, QString > defines(const QString&) const override
    {
        return {};
    }

    Path::List includes(const QString&) const override
    {
        return {};
    }
};

static CompilerPointer createDummyCompiler()
{
    static CompilerPointer compiler(new NoCompiler());
    return compiler;
}

ConfigEntry configForItem(KDevelop::ProjectBaseItem* item)
{
    if(!item){
        return ConfigEntry();
    }

    const Path itemPath = item->path();
    const Path rootDirectory = item->project()->path();

    auto paths = SettingsManager::globalInstance()->readPaths(item->project()->projectConfiguration().data());
    ConfigEntry config;
    Path closestPath;

    // find config entry closest to the requested item
    for (const auto& entry : paths) {
        auto configEntry = entry;
        Path targetDirectory = rootDirectory;

        targetDirectory.addPath(entry.path);

        if (targetDirectory == itemPath) {
            return configEntry;
        }

        if (targetDirectory.isParentOf(itemPath)) {
            if (config.path.isEmpty() || targetDirectory.segments().size() > closestPath.segments().size()) {
                config = configEntry;
                closestPath = targetDirectory;
            }
        }
    }

    return config;
}
}

CompilerProvider::CompilerProvider( SettingsManager* settings, QObject* parent )
    : QObject( parent )
    , m_settings(settings)
{
    m_factories.append(CompilerFactoryPointer(new GccFactory()));
    m_factories.append(CompilerFactoryPointer(new ClangFactory()));
#ifdef _WIN32
    m_factories.append(CompilerFactoryPointer(new MsvcFactory()));
#endif

    if (!QStandardPaths::findExecutable( "clang" ).isEmpty()) {
        m_factories[1]->registerDefaultCompilers(this);
    }
    if (!QStandardPaths::findExecutable( "gcc" ).isEmpty()) {
        m_factories[0]->registerDefaultCompilers(this);
    }
#ifdef _WIN32
    if (!QStandardPaths::findExecutable("cl.exe").isEmpty()) {
        m_factories[2]->registerDefaultCompilers(this);
    }
#endif

    registerCompiler(createDummyCompiler());
    retrieveUserDefinedCompilers();
}

CompilerProvider::~CompilerProvider() = default;

QHash<QString, QString> CompilerProvider::defines( ProjectBaseItem* item ) const
{
    auto config = configForItem(item);
    auto languageType = Utils::Cpp;
    if (item) {
        languageType = Utils::languageType(item->path(), config.parserArguments.parseAmbiguousAsCPP);
    }

    return config.compiler->defines(languageType == Utils::C ? config.parserArguments.cArguments : config.parserArguments.cppArguments);
}

Path::List CompilerProvider::includes( ProjectBaseItem* item ) const
{
    auto config = configForItem(item);
    auto languageType = Utils::Cpp;
    if (item) {
        languageType = Utils::languageType(item->path(), config.parserArguments.parseAmbiguousAsCPP);
    }

    return config.compiler->includes(languageType == Utils::C ? config.parserArguments.cArguments : config.parserArguments.cppArguments);
}

Path::List CompilerProvider::frameworkDirectories( ProjectBaseItem* /* item */ ) const
{
    return {};
}

IDefinesAndIncludesManager::Type CompilerProvider::type() const
{
    return IDefinesAndIncludesManager::CompilerSpecific;
}

CompilerPointer CompilerProvider::checkCompilerExists( const CompilerPointer& compiler ) const
{
    //This may happen for opened for the first time projects
    if ( !compiler ) {
        for ( auto& compiler : m_compilers ) {
            if ( QStandardPaths::findExecutable( compiler->path() ).isEmpty() ) {
                continue;
            }

            return compiler;
        }
    } else {
        for ( auto it = m_compilers.constBegin(); it != m_compilers.constEnd(); it++ ) {
            if ( (*it)->name() == compiler->name() ) {
                return *it;
            }
        }
    }

    return createDummyCompiler();
}

QVector< CompilerPointer > CompilerProvider::compilers() const
{
    return m_compilers;
}

CompilerPointer CompilerProvider::compilerForItem( KDevelop::ProjectBaseItem* item ) const
{
    auto compiler = configForItem(item).compiler;
    Q_ASSERT(compiler);
    return compiler;
}

bool CompilerProvider::registerCompiler(const CompilerPointer& compiler)
{
    if (!compiler) {
        return false;
    }

    for(auto c: m_compilers){
        if (c->name() == compiler->name()) {
            return false;
        }
    }
    m_compilers.append(compiler);
    return true;
}

void CompilerProvider::unregisterCompiler(const CompilerPointer& compiler)
{
    if (!compiler->editable()) {
        return;
    }

    for (int i = 0; i < m_compilers.count(); i++) {
        if (m_compilers[i]->name() == compiler->name()) {
            m_compilers.remove(i);
            break;
        }
    }
}

QVector< CompilerFactoryPointer > CompilerProvider::compilerFactories() const
{
    return m_factories;
}

void CompilerProvider::retrieveUserDefinedCompilers()
{
    auto compilers = m_settings->userDefinedCompilers();
    for (auto c : compilers) {
        registerCompiler(c);
    }
}

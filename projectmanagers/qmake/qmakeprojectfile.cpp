/* KDevelop QMake Support
 *
 * Copyright 2006 Andreas Pakulat <apaku@gmx.de>
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

#include "qmakeprojectfile.h"

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QDir>

#include <kprocess.h>

#include "debug.h"
#include "parser/ast.h"
#include "qmakecache.h"
#include "qmakemkspecs.h"
#include "qmakeconfig.h"

#include <interfaces/iproject.h>
#include <util/path.h>

#define ifDebug(x)

QHash<QString, QHash<QString, QString>> QMakeProjectFile::m_qmakeQueryCache;

const QStringList QMakeProjectFile::FileVariables = QStringList() << "IDLS"
                                                                  << "RESOURCES"
                                                                  << "IMAGES"
                                                                  << "LEXSOURCES"
                                                                  << "DISTFILES"
                                                                  << "YACCSOURCES"
                                                                  << "TRANSLATIONS"
                                                                  << "HEADERS"
                                                                  << "SOURCES"
                                                                  << "INTERFACES"
                                                                  << "FORMS";

QMakeProjectFile::QMakeProjectFile(const QString& projectfile)
    : QMakeFile(projectfile)
    , m_mkspecs(nullptr)
    , m_cache(nullptr)
{
}

void QMakeProjectFile::setQMakeCache(QMakeCache* cache)
{
    m_cache = cache;
}

void QMakeProjectFile::setMkSpecs(QMakeMkSpecs* mkspecs)
{
    m_mkspecs = mkspecs;
}

bool QMakeProjectFile::read()
{
    // default values
    // NOTE: if we already have such a var, e.g. in an include file, we must not overwrite it here!
    if (!m_variableValues.contains("QT")) {
        m_variableValues["QT"] = QStringList() << "core"
                                               << "gui";
    }
    if (!m_variableValues.contains("CONFIG")) {
        m_variableValues["CONFIG"] = QStringList() << "qt";
    }

    Q_ASSERT(m_mkspecs);
    foreach (const QString& var, m_mkspecs->variables()) {
        if (!m_variableValues.contains(var)) {
            m_variableValues[var] = m_mkspecs->variableValues(var);
        }
    }
    if (m_cache) {
        foreach (const QString& var, m_cache->variables()) {
            if (!m_variableValues.contains(var)) {
                m_variableValues[var] = m_cache->variableValues(var);
            }
        }
    }

    /// TODO: more special variables
    m_variableValues["PWD"] = QStringList() << pwd();
    m_variableValues["_PRO_FILE_"] = QStringList() << proFile();
    m_variableValues["_PRO_FILE_PWD_"] = QStringList() << proFilePwd();
    m_variableValues["OUT_PWD"] = QStringList() << outPwd();

    const QString qtInstallHeaders = QStringLiteral("QT_INSTALL_HEADERS");
    const QString qtVersion = QStringLiteral("QT_VERSION");
    const QString qtInstallLibs = QStringLiteral("QT_INSTALL_LIBS");

    const QString executable = QMakeConfig::qmakeExecutable(project());
    if (!m_qmakeQueryCache.contains(executable)) {
        const auto queryResult = QMakeConfig::queryQMake(executable, {qtInstallHeaders, qtVersion, qtInstallLibs});
        if (queryResult.isEmpty()) {
            qCWarning(KDEV_QMAKE) << "Failed to query qmake - bad qmake executable configured?" << executable;
        }
        m_qmakeQueryCache[executable] = queryResult;
    }

    const auto cachedQueryResult = m_qmakeQueryCache.value(executable);
    m_qtIncludeDir = cachedQueryResult.value(qtInstallHeaders);
    m_qtVersion = cachedQueryResult.value(qtVersion);
    m_qtLibDir = cachedQueryResult.value(qtInstallLibs);

    return QMakeFile::read();
}

QStringList QMakeProjectFile::subProjects() const
{
    ifDebug(qCDebug(KDEV_QMAKE) << "Fetching subprojects";) QStringList list;
    foreach (QString subdir, variableValues("SUBDIRS")) {
        QString fileOrPath;
        ifDebug(qCDebug(KDEV_QMAKE) << "Found value:" << subdir;) if (containsVariable(subdir + ".file")
                                                                      && !variableValues(subdir + ".file").isEmpty())
        {
            subdir = variableValues(subdir + ".file").first();
        }
        else if (containsVariable(subdir + ".subdir") && !variableValues(subdir + ".subdir").isEmpty())
        {
            subdir = variableValues(subdir + ".subdir").first();
        }
        if (subdir.endsWith(".pro")) {
            fileOrPath = resolveToSingleFileName(subdir.trimmed());
        } else {
            fileOrPath = resolveToSingleFileName(subdir.trimmed());
        }
        if (fileOrPath.isEmpty()) {
            qCWarning(KDEV_QMAKE) << "could not resolve subdir" << subdir << "to file or path, skipping";
            continue;
        }
        list << fileOrPath;
    }

    ifDebug(qCDebug(KDEV_QMAKE) << "found" << list.size() << "subprojects";) return list;
}

bool QMakeProjectFile::hasSubProject(const QString& file) const
{
    foreach (const QString& sub, subProjects()) {
        if (sub == file) {
            return true;
        } else if (QFileInfo(file).absoluteDir() == sub) {
            return true;
        }
    }
    return false;
}

void QMakeProjectFile::addPathsForVariable(const QString& variable, QStringList* list) const
{
    const QStringList values = variableValues(variable);
    ifDebug(qCDebug(KDEV_QMAKE) << variable << values;) foreach (const QString& val, values)
    {
        QString path = resolveToSingleFileName(val);
        if (!path.isEmpty() && !list->contains(val)) {
            list->append(path);
        }
    }
}

QStringList QMakeProjectFile::includeDirectories() const
{
    ifDebug(qCDebug(KDEV_QMAKE) << "Fetching include dirs" << m_qtIncludeDir;)
        ifDebug(qCDebug(KDEV_QMAKE) << "CONFIG" << variableValues("CONFIG");)

            QStringList list;
    addPathsForVariable("INCLUDEPATH", &list);
    addPathsForVariable("QMAKE_INCDIR", &list);
    if (variableValues("CONFIG").contains("opengl")) {
        addPathsForVariable("QMAKE_INCDIR_OPENGL", &list);
    }
    if (variableValues("CONFIG").contains("qt")) {
        if (!list.contains(m_qtIncludeDir))
            list << m_qtIncludeDir;

        QDir incDir(m_qtIncludeDir);
        auto modules = variableValues("QT");
        if (!modules.isEmpty() && !modules.contains("core")) {
            // TODO: proper dependency tracking of modules
            // for now, at least include core if we include any other module
            modules << "core";
        }

        // TODO: This is all very fragile, should rather read QMake module .pri files (e.g. qt_lib_core_private.pri)
        foreach (const QString& module, modules) {
            QString pattern = module;

            bool isPrivate = false;
            if (module.endsWith("-private")) {
                pattern.chop(qstrlen("-private"));
                isPrivate = true;
            } else if (module.endsWith("_private")) {
                // _private is less common, but still a valid suffix
                pattern.chop(qstrlen("_private"));
                isPrivate = true;
            }

            if (pattern == "qtestlib" || pattern == "testlib") {
                pattern = "QtTest";
            } else if (pattern == "qaxcontainer") {
                pattern = "ActiveQt";
            } else if (pattern == "qaxserver") {
                pattern = "ActiveQt";
            }

            QFileInfoList match = incDir.entryInfoList({QString("Qt%1").arg(pattern)}, QDir::Dirs);
            if (match.isEmpty()) {
                // try non-prefixed pattern
                match = incDir.entryInfoList({pattern}, QDir::Dirs);
                if (match.isEmpty()) {
                    qCWarning(KDEV_QMAKE) << "unhandled Qt module:" << module << pattern;
                    continue;
                }
            }

            QString path = match.first().canonicalFilePath();
            if (isPrivate) {
                path += '/' + m_qtVersion + '/' + match.first().fileName() + "/private/";
            }
            if (!list.contains(path)) {
                list << path;
            }
        }
    }

    if (variableValues("CONFIG").contains("thread")) {
        addPathsForVariable("QMAKE_INCDIR_THREAD", &list);
    }
    if (variableValues("CONFIG").contains("x11")) {
        addPathsForVariable("QMAKE_INCDIR_X11", &list);
    }

    addPathsForVariable("MOC_DIR", &list);
    addPathsForVariable("OBJECTS_DIR", &list);
    addPathsForVariable("UI_DIR", &list);

    ifDebug(qCDebug(KDEV_QMAKE) << "final list:" << list;) return list;
}

// Scan QMAKE_C*FLAGS for -F and -iframework and QMAKE_LFLAGS for good measure. Time will
// tell if we need to scan the release/debug/... specific versions of QMAKE_C*FLAGS.
// Also include QT_INSTALL_LIBS which corresponds to Qt's framework directory on OS X.
QStringList QMakeProjectFile::frameworkDirectories() const
{
    const auto variablesToCheck = {QStringLiteral("QMAKE_CFLAGS"),
                                    QStringLiteral("QMAKE_CXXFLAGS"),
                                    QStringLiteral("QMAKE_LFLAGS")};
    const QLatin1String fOption("-F");
    const QLatin1String iframeworkOption("-iframework");
    QStringList fwDirs;
    foreach (const auto& var, variablesToCheck) {
        bool storeArg = false;
        foreach (const auto& arg, variableValues(var)) {
            if (arg == fOption || arg == iframeworkOption) {
                // detached -F/-iframework arg; set a warrant to store the next argument
                storeArg = true;
            } else {
                if (arg.startsWith(fOption)) {
                    fwDirs << arg.mid(fOption.size());
                } else if (arg.startsWith(iframeworkOption)) {
                    fwDirs << arg.mid(iframeworkOption.size());
                } else if (storeArg) {
                    fwDirs << arg;
                }
                // cancel any outstanding warrants to store the next argument
                storeArg = false;
            }
        }
    }
#ifdef Q_OS_OSX
    fwDirs << m_qtLibDir;
#endif
    return fwDirs;
}

QStringList QMakeProjectFile::files() const
{
    ifDebug(qCDebug(KDEV_QMAKE) << "Fetching files";)

        QStringList list;
    foreach (const QString& variable, QMakeProjectFile::FileVariables) {
        foreach (const QString& value, variableValues(variable)) {
            list += resolveFileName(value);
        }
    }
    ifDebug(qCDebug(KDEV_QMAKE) << "found" << list.size() << "files";) return list;
}

QStringList QMakeProjectFile::filesForTarget(const QString& s) const
{
    ifDebug(qCDebug(KDEV_QMAKE) << "Fetching files";)

        QStringList list;
    if (variableValues("INSTALLS").contains(s)) {
        const QStringList files = variableValues(s + ".files");
        if (!files.isEmpty()) {
            foreach (const QString& val, files) {
                list += QStringList(resolveFileName(val));
            }
        }
    }
    if (!variableValues("INSTALLS").contains(s) || s == "target") {
        foreach (const QString& variable, QMakeProjectFile::FileVariables) {
            foreach (const QString& value, variableValues(variable)) {
                list += QStringList(resolveFileName(value));
            }
        }
    }
    ifDebug(qCDebug(KDEV_QMAKE) << "found" << list.size() << "files";) return list;
}

QString QMakeProjectFile::getTemplate() const
{
    QString templ = "app";
    if (!variableValues("TEMPLATE").isEmpty()) {
        templ = variableValues("TEMPLATE").first();
    }
    return templ;
}

QStringList QMakeProjectFile::targets() const
{
    ifDebug(qCDebug(KDEV_QMAKE) << "Fetching targets";)

        QStringList list;

    list += variableValues("TARGET");
    if (list.isEmpty() && getTemplate() != "subdirs") {
        list += QFileInfo(absoluteFile()).baseName();
    }

    foreach (const QString& target, variableValues("INSTALLS")) {
        if (!target.isEmpty() && target != "target")
            list << target;
    }

    if (list.removeAll(QString())) {
        // remove empty targets - which is probably a bug...
        qCWarning(KDEV_QMAKE) << "got empty entry in TARGET of file" << absoluteFile();
    }

    ifDebug(qCDebug(KDEV_QMAKE) << "found" << list.size() << "targets";) return list;
}

QMakeProjectFile::~QMakeProjectFile()
{
    // TODO: delete cache, specs, ...?
}

QStringList QMakeProjectFile::resolveVariable(const QString& variable, VariableInfo::VariableType type) const
{
    if (type == VariableInfo::QtConfigVariable) {
        if (m_mkspecs->isQMakeInternalVariable(variable)) {
            return QStringList() << m_mkspecs->qmakeInternalVariable(variable);
        } else {
            qCWarning(KDEV_QMAKE) << "unknown QtConfig Variable:" << variable;
            return QStringList();
        }
    }

    return QMakeFile::resolveVariable(variable, type);
}

QMakeMkSpecs* QMakeProjectFile::mkSpecs() const
{
    return m_mkspecs;
}

QMakeCache* QMakeProjectFile::qmakeCache() const
{
    return m_cache;
}

QList<QMakeProjectFile::DefinePair> QMakeProjectFile::defines() const
{
    QList<DefinePair> d;
    foreach (QString def, variableMap()["DEFINES"]) {
        int pos = def.indexOf('=');
        if (pos >= 0) {
            // a value is attached to define
            d.append(DefinePair(def.left(pos), def.right(def.length() - (pos + 1))));
        } else {
            // a value-less define
            d.append(DefinePair(def, ""));
        }
    }
    return d;
}

QString QMakeProjectFile::pwd() const
{
    return absoluteDir();
}

QString QMakeProjectFile::outPwd() const
{
    if (!project()) {
        return absoluteDir();
    } else {
        return QMakeConfig::buildDirFromSrc(project(), KDevelop::Path(absoluteDir())).toLocalFile();
    }
}

QString QMakeProjectFile::proFile() const
{
    return absoluteFile();
}

QString QMakeProjectFile::proFilePwd() const
{
    return absoluteDir();
}

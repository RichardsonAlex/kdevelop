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

#include "gcclikecompiler.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QMap>

#include <KShell>

#include "../debugarea.h"

using namespace KDevelop;

namespace
{
// compilers don't deduplicate QStringLiteral
QString minusXC() { return QStringLiteral("-xc"); }
QString minusXCPlusPlus() { return QStringLiteral("-xc++"); }

QStringList languageOptions(const QStringList& arguments)
{
    QString language;
    bool languageDefined = false;
    QString standardFlag;
    QStringList result = arguments;
    // TODO: handle -ansi flag: In C mode, this is equivalent to -std=c90. In C++ mode, it is equivalent to -std=c++98.
    // TODO: check for -x flag on command line
    for (int i = 0; i < arguments.size(); ++i) {
        const QString& arg = arguments.at(i);
        if (arg.startsWith(QLatin1String("-std="))) {
            standardFlag = arg;
            QStringRef standardVersion = arg.midRef(strlen("-std="));
            if (standardVersion.startsWith(QLatin1String("c++")) || standardVersion.startsWith(QLatin1String("gnu++"))) {
                language = minusXCPlusPlus();
            } else if (standardVersion.startsWith(QLatin1String("iso9899:"))) {
                // all iso9899:xxxxx modes are C standards
                language = minusXC();
            } else {
                // check for c11, gnu99, etc: all of them have a digit after the c/gnu
                const QRegularExpression cRegexp("(c|gnu)\\d.*");
                if (cRegexp.match(standardVersion).hasMatch()) {
                    language = minusXC();
                }
                if (language.isEmpty()) {
                    qCWarning(DEFINESANDINCLUDES) << "Failed to determine language from -std= flag:" << arguments;
                }
            }
        } else if (arg == QLatin1String("-ansi")) {
            standardFlag = arg; // depends on whether we compile C or C++
        } else if (arg.startsWith(QLatin1String("-x"))) {
            languageDefined = true; // No need to pass an additional language flag
            if (arg == QLatin1String("-x")) {
                // it is split into two arguments -> next argument is the language
                if (i + 1 < arguments.size()) {
                    language = arg + arguments[i + 1];
                }
            } else {
                language = arg;
            }
        }
    }
    // We need to pass a language option because the compiler will error out when reading /dev/null without a -x option
    if (!languageDefined) {
        if (language.isEmpty()) {
            if (standardFlag.isEmpty()) {
                standardFlag = QStringLiteral("-std=c++11");
                result.append(standardFlag);
            }
            qCWarning(DEFINESANDINCLUDES) << "Could not detect language from"
                    << arguments << "-> assuming C++ with " << standardFlag;
            language = minusXCPlusPlus();
        }
        qCWarning(DEFINESANDINCLUDES) << "Lang+Standard for" << arguments << "are:" << standardFlag << language;
        result.append(language);
    }
    return result;
}
}

Defines GccLikeCompiler::defines(const QString& arguments) const
{
    if (!m_definesIncludes.value(arguments).definedMacros.isEmpty() ) {
        return m_definesIncludes.value(arguments).definedMacros;
    }
    // FIXME: thread safety???

    // #define a 1
    // #define a
    QRegExp defineExpression( "#define\\s+(\\S+)(?:\\s+(.*)\\s*)?");

    QProcess proc;
    proc.setProcessChannelMode( QProcess::MergedChannels );

    // We need to pass all arguments as e.g. -m and -f flags change the default defines
    const QStringList splitArguments = KShell::splitArgs(additionalArguments() + QLatin1Char(' ') + arguments);
    auto compilerArguments = languageOptions(splitArguments);
    compilerArguments.append("-dM");
    compilerArguments.append("-E");
    compilerArguments.append(QProcess::nullDevice());

    proc.start(path(), compilerArguments);

    if ( !proc.waitForStarted( 1000 ) || !proc.waitForFinished( 1000 ) ) {
        definesAndIncludesDebug() <<  "Unable to read standard macro definitions from "<< path();
        return {};
    }

    while ( proc.canReadLine() ) {
        auto line = proc.readLine();

        if ( defineExpression.indexIn( line ) != -1 ) {
            m_definesIncludes[arguments].definedMacros[defineExpression.cap( 1 )] = defineExpression.cap( 2 ).trimmed();
        }

    }
    definesAndIncludesDebug() << "defines for:" << path() << compilerArguments << m_definesIncludes[arguments].definedMacros;

    return m_definesIncludes[arguments].definedMacros;
}

Path::List GccLikeCompiler::includes(const QString& arguments) const
{
    if ( !m_definesIncludes.value(arguments).includePaths.isEmpty() ) {
        return m_definesIncludes.value(arguments).includePaths;
    }

    // FIXME: thread safety???

    QProcess proc;
    proc.setProcessChannelMode( QProcess::MergedChannels );

    // The following command will spit out a bunch of information we don't care
    // about before spitting out the include paths.  The parts we care about
    // look like this:
    // #include "..." search starts here:
    // #include <...> search starts here:
    //  /usr/lib/gcc/i486-linux-gnu/4.1.2/../../../../include/c++/4.1.2
    //  /usr/lib/gcc/i486-linux-gnu/4.1.2/../../../../include/c++/4.1.2/i486-linux-gnu
    //  /usr/lib/gcc/i486-linux-gnu/4.1.2/../../../../include/c++/4.1.2/backward
    //  /usr/local/include
    //  /usr/lib/gcc/i486-linux-gnu/4.1.2/include
    //  /usr/include
    // End of search list.

    const QStringList splitArguments = KShell::splitArgs(additionalArguments() + QLatin1Char(' ') + arguments);
    auto compilerArguments = languageOptions(splitArguments);
    compilerArguments.append("-E");
    compilerArguments.append("-v");
    compilerArguments.append(QProcess::nullDevice());

    proc.start(path(), compilerArguments);

    if ( !proc.waitForStarted( 1000 ) || !proc.waitForFinished( 1000 ) ) {
        definesAndIncludesDebug() <<  "Unable to read standard include paths from " << path();
        return {};
    }

    // We'll use the following constants to know what we're currently parsing.
    enum Status {
        Initial,
        FirstSearch,
        Includes,
        Finished
    };
    Status mode = Initial;

    foreach( const QString &line, QString::fromLocal8Bit( proc.readAllStandardOutput() ).split( '\n' ) ) {
        switch ( mode ) {
            case Initial:
                if ( line.indexOf( "#include \"...\"" ) != -1 ) {
                    mode = FirstSearch;
                }
                break;
            case FirstSearch:
                if ( line.indexOf( "#include <...>" ) != -1 ) {
                    mode = Includes;
                    break;
                }
            case Includes:
                //Detect the include-paths by the first space that is prepended. Reason: The list may contain relative paths like "."
                if ( !line.startsWith( ' ' ) ) {
                    // We've reached the end of the list.
                    mode = Finished;
                } else {
                    // This is an include path, add it to the list.
                    m_definesIncludes[arguments].includePaths << Path(QFileInfo(line.trimmed()).canonicalFilePath());
                }
                break;
            default:
                break;
        }
        if ( mode == Finished ) {
            break;
        }
    }
    definesAndIncludesDebug() << "includes for:" << path() << compilerArguments << m_definesIncludes[arguments].includePaths;

    return m_definesIncludes[arguments].includePaths;
}

GccLikeCompiler::GccLikeCompiler(const QString& name, const QString& path, const QString& additionalArguments, bool editable, const QString& factoryName):
    ICompiler(name, path, additionalArguments, factoryName, editable)
{}

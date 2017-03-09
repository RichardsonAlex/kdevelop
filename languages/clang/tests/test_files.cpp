/*************************************************************************************
 *  Copyright (C) 2013 by Milian Wolff <mail@milianw.de>                             *
 *  Copyright (C) 2013 Olivier de Gaalon <olivier.jg@gmail.com>                      *
 *                                                                                   *
 *  This program is free software; you can redistribute it and/or                    *
 *  modify it under the terms of the GNU General Public License                      *
 *  as published by the Free Software Foundation; either version 2                   *
 *  of the License, or (at your option) any later version.                           *
 *                                                                                   *
 *  This program is distributed in the hope that it will be useful,                  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
 *  GNU General Public License for more details.                                     *
 *                                                                                   *
 *  You should have received a copy of the GNU General Public License                *
 *  along with this program; if not, write to the Free Software                      *
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA   *
 *************************************************************************************/

#include "test_files.h"

#include <language/duchain/duchain.h>
#include <language/duchain/problem.h>
#include <language/codegen/coderepresentation.h>
#include <language/backgroundparser/backgroundparser.h>

#include <tests/testcore.h>
#include <tests/autotestshell.h>
#include <tests/json/declarationvalidator.h>

#include "testfilepaths.h"

//Include all used json tests, otherwise "Test not found"
#include <tests/json/jsondeclarationtests.h>
#include <tests/json/jsonducontexttests.h>
#include <tests/json/jsontypetests.h>
#include <interfaces/ilanguagecontroller.h>

#include <QtTest>

using namespace KDevelop;

QTEST_MAIN(TestFiles)

void TestFiles::initTestCase()
{
    qputenv("KDEV_CLANG_JSON_TEST_RUN", "1");
    qputenv("KDEV_CLANG_EXTRA_ARGUMENTS", "-Wno-unused-variable -Wno-unused-parameter -Wno-unused-comparison -Wno-unused-value -Wno-unused-private-field -Wno-ignored-attributes");

    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\ndefault.debug=true\nkdevelop.plugins.clang.debug=true\n"));
    QVERIFY(qputenv("KDEV_DISABLE_PLUGINS", "kdevcppsupport"));
    QVERIFY(qputenv("KDEV_CLANG_DISPLAY_DIAGS", "1"));
    AutoTestShell::init({"kdevclangsupport"});
    TestCore::initialize(Core::NoUi);
    DUChain::self()->disablePersistentStorage();
    Core::self()->languageController()->backgroundParser()->setDelay(0);
    CodeRepresentation::setDiskChangesForbidden(true);
}

void TestFiles::cleanupTestCase()
{
    TestCore::shutdown();
}

void TestFiles::testFiles_data()
{
    QTest::addColumn<QString>("fileName");
    const QString testDirPath = TEST_FILES_DIR;
    const QStringList files = QDir(testDirPath).entryList({"*.h", "*.cpp", "*.c", "*.cl"}, QDir::Files);
    foreach (const QString& file, files) {
        QTest::newRow(file.toUtf8().constData()) << QString(testDirPath + '/' + file);
    }
}

void TestFiles::testFiles()
{
    QFETCH(QString, fileName);
    const IndexedString indexedFileName(fileName);
    ReferencedTopDUContext top =
        DUChain::self()->waitForUpdate(indexedFileName, TopDUContext::AllDeclarationsContextsAndUses);
    QVERIFY(top);
    DUChainReadLocker lock;
    DeclarationValidator validator;
    top->visit(validator);
    QVERIFY(validator.testsPassed());

    foreach(auto problem, top->problems()) {
        qDebug() << problem;
    }

    if (!QTest::currentDataTag() || strcmp("invalid.cpp", QTest::currentDataTag()) != 0) {
        QVERIFY(top->problems().isEmpty());
    }
}

/*
 * This file is part of qmljs, the QML/JS language support plugin for KDevelop
 * Copyright (c) 2013 Sven Brauch <svenbrauch@googlemail.com>
 * Copyright (c) 2014 Denis Steckelmacher <steckdenis@yahoo.fr>
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
#include "qmljsducontext.h"
#include "debug.h"

#include <language/duchain/topducontext.h>
#include <language/duchain/topducontextdata.h>
#include <language/duchain/duchainregister.h>
#include <language/duchain/duchainpointer.h>
#include <language/util/includeitem.h>

#include "navigation/navigationwidget.h"

using namespace KDevelop;

namespace QmlJS {

template<>
QWidget* QmlJSTopDUContext::createNavigationWidget(Declaration* decl, TopDUContext* topContext,
                                                   const QString& htmlPrefix, const QString& htmlSuffix,
                                                   AbstractNavigationWidget::DisplayHints hints) const
{
    if (!decl) {
        const QUrl u = url().toUrl();
        IncludeItem item;
        item.pathNumber = -1;
        item.name = u.fileName();
        item.isDirectory = false;
        item.basePath = u.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);

        return new NavigationWidget(item, TopDUContextPointer(topContext ? topContext : this->topContext()), htmlPrefix, htmlSuffix, hints);
    }
    return new NavigationWidget(decl, topContext, htmlPrefix, htmlSuffix, hints);
}

template<>
QWidget* QmlJSNormalDUContext::createNavigationWidget(Declaration* decl, TopDUContext* topContext, const QString& htmlPrefix,
                                                      const QString& htmlSuffix, AbstractNavigationWidget::DisplayHints hints) const {
    if (!decl) {
        qCDebug(KDEV_QMLJS_DUCHAIN) << "no declaration, not returning navigationwidget";
        return nullptr;
    }
    return new NavigationWidget(decl, topContext, htmlPrefix, htmlSuffix, hints);
}

}

DUCHAIN_DEFINE_TYPE_WITH_DATA(QmlJS::QmlJSNormalDUContext, DUContextData)
DUCHAIN_DEFINE_TYPE_WITH_DATA(QmlJS::QmlJSTopDUContext, TopDUContextData)

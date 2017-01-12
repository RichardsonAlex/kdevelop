/*  This file is part of KDevelop
    Copyright 2009 Aleix Pol <aleixpol@kde.org>
    Copyright 2009 David Nolden <david.nolden.kdevelop@art-master.de>
    Copyright 2010 Benjamin Port <port.benjamin@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "qthelpdocumentation.h"

#include <QLabel>
#include <QUrl>
#include <QTreeView>
#include <QHelpContentModel>
#include <QHeaderView>
#include <QMenu>
#include <QTemporaryFile>
#include <QRegularExpression>

#include <KLocalizedString>

#include <interfaces/icore.h>
#include <interfaces/idocumentationcontroller.h>
#include "qthelpnetwork.h"
#include "qthelpproviderabstract.h"

using namespace KDevelop;

namespace {
#if QT_VERSION >= 0x050500
int indexOf(const QString& str, const QRegularExpression& re, int from, QRegularExpressionMatch* rmatch)
{
    return str.indexOf(re, from, rmatch);
}

int lastIndexOf(const QString& str, const QRegularExpression& re, int from, QRegularExpressionMatch* rmatch)
{
    return str.lastIndexOf(re, from, rmatch);
}
#else
int indexOf(const QString& str, const QRegularExpression& re, int from, QRegularExpressionMatch* rmatch)
{
    if (!re.isValid()) {
        qWarning("QString::indexOf: invalid QRegularExpression object");
        return -1;
    }

    QRegularExpressionMatch match = re.match(str, from);
    if (match.hasMatch()) {
        const int ret = match.capturedStart();
        if (rmatch)
            *rmatch = qMove(match);
        return ret;
    }

    return -1;
}

int lastIndexOf(const QString &str, const QRegularExpression &re, int from, QRegularExpressionMatch *rmatch)
{
    if (!re.isValid()) {
        qWarning("QString::lastIndexOf: invalid QRegularExpression object");
        return -1;
    }

    int endpos = (from < 0) ? (str.size() + from + 1) : (from + 1);
    QRegularExpressionMatchIterator iterator = re.globalMatch(str);
    int lastIndex = -1;
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        int start = match.capturedStart();
        if (start < endpos) {
            lastIndex = start;
            if (rmatch)
                *rmatch = qMove(match);
        } else {
            break;
        }
    }

    return lastIndex;
}
#endif

}

QtHelpProviderAbstract* QtHelpDocumentation::s_provider=nullptr;

QtHelpDocumentation::QtHelpDocumentation(const QString& name, const QMap<QString, QUrl>& info)
    : m_provider(s_provider), m_name(name), m_info(info), m_current(info.constBegin()), lastView(nullptr)
{}

QtHelpDocumentation::QtHelpDocumentation(const QString& name, const QMap<QString, QUrl>& info, const QString& key)
    : m_provider(s_provider), m_name(name), m_info(info), m_current(m_info.find(key)), lastView(nullptr)
{ Q_ASSERT(m_current!=m_info.constEnd()); }

QtHelpDocumentation::~QtHelpDocumentation()
{
}

QString QtHelpDocumentation::description() const
{
    QUrl url(m_current.value());
    QByteArray data = m_provider->engine()->fileData(url);

    //Extract a short description from the html data
    const QString dataString = QString::fromLatin1(data); ///@todo encoding

    const QString fragment = url.fragment();
    const QString p = QStringLiteral("((\\\")|(\\\'))");
    const QString optionalSpace = QStringLiteral("( )*");
    const QString exp = QString("< a name = " + p + fragment + p + " > < / a >").replace(' ', optionalSpace);

    const QRegularExpression findFragment(exp);
    QRegularExpressionMatch findFragmentMatch;
    int pos = indexOf(dataString, findFragment, 0, &findFragmentMatch);

    if(fragment.isEmpty()) {
        pos = 0;
    } else {

        //Check if there is a title opening-tag right before the fragment, and if yes add it, so we have a nicely formatted caption
        const QString titleRegExp = QStringLiteral("< h\\d class = \".*\" >").replace(' ', optionalSpace);
        const QRegularExpression findTitle(titleRegExp);
        const QRegularExpressionMatch match = findTitle.match(dataString, pos);
        const int titleStart = match.capturedStart();
        const int titleEnd = titleStart + match.capturedEnd();
        if(titleStart != -1) {
            const QStringRef between = dataString.midRef(titleEnd, pos-titleEnd).trimmed();
            if(between.isEmpty())
                pos = titleStart;
        }
    }

    if(pos != -1) {
        const QString exp = QString("< a name = " + p + "((\\S)*)" + p + " > < / a >").replace(' ', optionalSpace);
        const QRegularExpression nextFragmentExpression(exp);
        int endPos = dataString.indexOf(nextFragmentExpression, pos+(fragment.size() ? findFragmentMatch.capturedLength() : 0));
        if(endPos == -1) {
            endPos = dataString.size();
        }

        {
            //Find the end of the last paragraph or newline, so we don't add prefixes of the following fragment
            const QString newLineRegExp = QStringLiteral ("< br / > | < / p >").replace(' ', optionalSpace);
            const QRegularExpression lastNewLine(newLineRegExp);
            QRegularExpressionMatch match;
            const int newEnd = lastIndexOf(dataString, lastNewLine, endPos, &match);
            if(match.isValid() && newEnd > pos)
                endPos = newEnd + match.capturedLength();
        }

        {
            //Find the title, and start from there
            const QString titleRegExp = QStringLiteral("< h\\d class = \"title\" >").replace(' ', optionalSpace);
            const QRegularExpression findTitle(titleRegExp);
            const QRegularExpressionMatch match = findTitle.match(dataString);
            if (match.isValid())
                pos = qBound(pos, match.capturedStart(), endPos);
        }


        QString thisFragment = dataString.mid(pos, endPos - pos);

        {
            //Completely remove the first large header found, since we don't need a header
            const QString headerRegExp = QStringLiteral("< h\\d.*>.*?< / h\\d >").replace(' ', optionalSpace);
            const QRegularExpression findHeader(headerRegExp);
            const QRegularExpressionMatch match = findHeader.match(thisFragment);
            if(match.isValid()) {
                thisFragment.remove(match.capturedStart(), match.capturedLength());
            }
        }

        {
            //Replace all gigantic header-font sizes with <big>
            {
                const QString sizeRegExp = QStringLiteral("< h\\d ").replace(' ', optionalSpace);
                const QRegularExpression findSize(sizeRegExp);
                thisFragment.replace(findSize, "<big ");
            }
            {
                const QString sizeCloseRegExp = QStringLiteral("< / h\\d >").replace(' ', optionalSpace);
                const QRegularExpression closeSize(sizeCloseRegExp);
                thisFragment.replace(closeSize, "</big><br />");
            }
        }

        {
            //Replace paragraphs by newlines
            const QString begin = QStringLiteral("< p >").replace(' ', optionalSpace);
            const QRegularExpression findBegin(begin);
            thisFragment.replace(findBegin, {});

            const QString end = QStringLiteral("< /p >").replace(' ', optionalSpace);
            const QRegularExpression findEnd(end);
            thisFragment.replace(findEnd, "<br />");
        }

        {
            //Remove links, because they won't work
            const QString link = QString("< a href = " + p + ".*?" + p).replace(' ', optionalSpace);
            const QRegularExpression exp(link, QRegularExpression::CaseInsensitiveOption);
            thisFragment.replace(exp, "<a ");
        }

        return thisFragment;
    }

    return QStringList(m_info.keys()).join(", ");
}

void QtHelpDocumentation::setUserStyleSheet(StandardDocumentationView* view, const QUrl& url)
{

#if HAVE_QTWEBKIT
    QTemporaryFile* file = new QTemporaryFile(view);
    file->open();

    QTextStream ts(file);
#else
    QString cssString;
    QTextStream ts(&cssString);
#endif
    ts << "html { background: white !important; }\n";
    if (url.scheme() == "qthelp" && url.host().startsWith("com.trolltech.qt.")) {
       ts << ".content .toc + .title + p { clear:left; }\n"
          << "#qtdocheader .qtref { position: absolute !important; top: 5px !important; right: 0 !important; }\n";
    }
#if HAVE_QTWEBKIT
    file->close();
    view->settings()->setUserStyleSheetUrl(QUrl::fromLocalFile(file->fileName()));
    delete m_lastStyleSheet.data();
    m_lastStyleSheet = file;
#else
    ts.flush();
    qWarning() << "Setting css to" << cssString;
    view->document()->setDefaultStyleSheet(cssString);
#endif

}

QWidget* QtHelpDocumentation::documentationWidget(DocumentationFindWidget* findWidget, QWidget* parent)
{
    if(m_info.isEmpty()) { //QtHelp sometimes has empty info maps. e.g. availableaudioeffects i 4.5.2
        return new QLabel(i18n("Could not find any documentation for '%1'", m_name), parent);
    } else {
        StandardDocumentationView* view = new StandardDocumentationView(findWidget, parent);
#if HAVE_QTWEBKIT
        // QCH files created by doxygen can come with JavaScript
        view->settings()->setAttribute(QWebSettings::JavascriptEnabled, true);
        if (!m_sharedQNAM) {
            m_sharedQNAM.reset(new HelpNetworkAccessManager(m_provider->engine()));
        }
        view->page()->setNetworkAccessManager(m_sharedQNAM.data());
        view->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &StandardDocumentationView::customContextMenuRequested, this, &QtHelpDocumentation::viewContextMenuRequested);
        connect(view, &StandardDocumentationView::linkClicked, this, &QtHelpDocumentation::jumpedTo);
#else
        connect(view, &StandardDocumentationView::anchorClicked, this, &QtHelpDocumentation::jumpedTo);
#endif
        setUserStyleSheet(view, m_current.value());

        view->setHtml(m_provider->engine()->fileData(m_current.value()));
#if HAVE_QTWEBKIT
        view->setUrl(m_current.value());
#endif
        lastView = view;
        return view;
    }
}

void QtHelpDocumentation::viewContextMenuRequested(const QPoint& pos)
{
#if HAVE_QTWEBKIT
    StandardDocumentationView* view = qobject_cast<StandardDocumentationView*>(sender());
    if (!view)
        return;

    QMenu menu;
    QAction* copyAction = view->pageAction(QWebPage::Copy);
    copyAction->setIcon(QIcon::fromTheme("edit-copy"));
    menu.addAction(copyAction);

    if (m_info.count() > 1) {
        menu.addSeparator();

        QActionGroup* actionGroup = new QActionGroup(&menu);
        foreach(const QString& name, m_info.keys()) {
            QtHelpAlternativeLink* act=new QtHelpAlternativeLink(name, this, actionGroup);
            act->setCheckable(true);
            act->setChecked(name==m_current.key());
            menu.addAction(act);
        }
    }

    menu.exec(view->mapToGlobal(pos));
#endif
}


void QtHelpDocumentation::jumpedTo(const QUrl& newUrl)
{
    Q_ASSERT(lastView);
    m_provider->jumpedTo(newUrl);
    setUserStyleSheet(lastView, newUrl);
#if HAVE_QTWEBKIT
    lastView->load(newUrl);
#else
    lastView->setSource(newUrl);
#endif
}

IDocumentationProvider* QtHelpDocumentation::provider() const
{
    return m_provider;
}

QtHelpAlternativeLink::QtHelpAlternativeLink(const QString& name, const QtHelpDocumentation* doc, QObject* parent)
    : QAction(name, parent), mDoc(doc), mName(name)
{
    connect(this, &QtHelpAlternativeLink::triggered, this, &QtHelpAlternativeLink::showUrl);
}

void QtHelpAlternativeLink::showUrl()
{
    IDocumentation::Ptr newDoc(new QtHelpDocumentation(mName, mDoc->info(), mName));
    ICore::self()->documentationController()->showDocumentation(newDoc);
}

HomeDocumentation::HomeDocumentation() : m_provider(QtHelpDocumentation::s_provider)
{
}

QWidget* HomeDocumentation::documentationWidget(DocumentationFindWidget*, QWidget* parent)
{
    QTreeView* w=new QTreeView(parent);
    w->header()->setVisible(false);
    w->setModel(m_provider->engine()->contentModel());

    connect(w, &QTreeView::clicked, this, &HomeDocumentation::clicked);
    return w;
}

void HomeDocumentation::clicked(const QModelIndex& idx)
{
    QHelpContentModel* model = m_provider->engine()->contentModel();
    QHelpContentItem* it=model->contentItemAt(idx);
    QMap<QString, QUrl> info;
    info.insert(it->title(), it->url());

    IDocumentation::Ptr newDoc(new QtHelpDocumentation(it->title(), info));
    ICore::self()->documentationController()->showDocumentation(newDoc);
}

QString HomeDocumentation::name() const
{
    return i18n("QtHelp Home Page");
}

IDocumentationProvider* HomeDocumentation::provider() const
{
    return m_provider;
}

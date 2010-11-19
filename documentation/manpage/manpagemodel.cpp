/*  This file is part of KDevelop

    Copyright 2010 Yannick Motta <yannick.motta@gmail.com>
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

#include "manpagemodel.h"

#include <language/duchain/duchain.h>
#include <language/duchain/declaration.h>
#include <language/duchain/duchainlock.h>

#include <language/duchain/types/structuretype.h>

#include <interfaces/icore.h>
#include <interfaces/ilanguage.h>
#include <interfaces/ilanguagecontroller.h>
#include <language/backgroundparser/backgroundparser.h>
#include <language/backgroundparser/parsejob.h>

#include <KStandardDirs>
#include <KLocalizedString>

#include <KIO/TransferJob>
#include <KIO/Job>
#include <kio/jobclasses.h>

#include <QWebPage>
#include <QWebFrame>
#include <QWebElement>
#include <QWebElementCollection>

#include <QtDebug>
#include <QTreeView>
#include <QHeaderView>

using namespace KDevelop;

ManPageModel::ManPageModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    initModel();
}


QModelIndex ManPageModel::parent(const QModelIndex& child) const
{
    if(child.isValid() && child.column()==0 && int(child.internalId())>=0)
        return createIndex(child.internalId(),0, -1);
    return QModelIndex();
}

QModelIndex ManPageModel::index(int row, int column, const QModelIndex& parent) const
{
    if(row<0 || column!=0)
        return QModelIndex();
    if(!parent.isValid() && row==m_sectionList.count())
        return QModelIndex();

    return createIndex(row,column, int(parent.isValid() ? parent.row() : -1));
}

QVariant ManPageModel::data(const QModelIndex& index, int role) const
{
    if(index.isValid()){
        if(role==Qt::DisplayRole) {
            int internal(index.internalId());
            if(internal>=0)
                return QVariant();
            else
                return m_sectionList.at(index.row()).second;
        }
    }
    return QVariant();
}

int ManPageModel::rowCount(const QModelIndex& parent) const
{
    if(!parent.isValid()){
        return m_sectionList.count();
    }else if(int(parent.internalId())<0) {
        return 0;
    }
    return 0;
}

void ManPageModel::getManPage(const KUrl& page){
    KIO::TransferJob  * transferJob = NULL;
    //page = KUrl("man:/usr/share/man/man3/a64l.3.gz");

    transferJob = KIO::get(KUrl("man://"), KIO::NoReload, KIO::HideProgressInfo);
    connect( transferJob, SIGNAL( data  (  KIO::Job *, const QByteArray &)),
             this, SLOT( readDataFromManPage( KIO::Job *, const QByteArray & ) ) );

}

void ManPageModel::initModel(){
    m_manMainIndexBuffer.clear();
    KIO::TransferJob  * transferJob = 0;

    transferJob = KIO::get(KUrl("man://"), KIO::NoReload, KIO::HideProgressInfo);
    connect( transferJob, SIGNAL( data  (  KIO::Job *, const QByteArray &)),
             this, SLOT( readDataFromMainIndex( KIO::Job *, const QByteArray & ) ) );

    if (transferJob->exec()){
        m_sectionList = this->indexParser();
    } else {
        qDebug() << "ManPageModel transferJob error";
    }
    foreach(ManSection section, m_sectionList){
        initSection(section.first);
    }
}

void ManPageModel::initSection(const QString sectionId){
    m_manSectionIndexBuffer.clear();
    KIO::TransferJob  * transferJob = 0;

    transferJob = KIO::get(KUrl("man:(" + sectionId + ")"), KIO::NoReload, KIO::HideProgressInfo);
    connect( transferJob, SIGNAL( data  (  KIO::Job *, const QByteArray &)),
             this, SLOT( readDataFromSectionIndex( KIO::Job *, const QByteArray & ) ) );

    if (transferJob->exec()){
        m_manMap = this->sectionParser(sectionId);
    } else {
        qDebug() << "ManPageModel transferJob error";
    }
}

void ManPageModel::readDataFromManPage(KIO::Job * job, const QByteArray &data){
     m_manPageBuffer.append(data);
}

void ManPageModel::readDataFromMainIndex(KIO::Job * job, const QByteArray &data){
     m_manMainIndexBuffer.append(data);
}

void ManPageModel::readDataFromSectionIndex(KIO::Job * job, const QByteArray &data){
     m_manSectionIndexBuffer.append(data);
}

QList<ManSection> ManPageModel::indexParser(){

     QWebPage * page = new QWebPage();
     QWebFrame * frame = page->mainFrame();
     frame->setHtml(m_manMainIndexBuffer);
     QWebElement document = frame->documentElement();
     QWebElementCollection links = document.findAll("a");
     QList<ManSection> list;
     foreach(QWebElement e, links){
        list.append(qMakePair(e.attribute("accesskey"), e.toPlainText()));
     }
     return list;
}

QMap<ManPage, QString> ManPageModel::sectionParser(const QString &sectionId){
     QWebPage * page = new QWebPage();
     QWebFrame * frame = page->mainFrame();
     frame->setHtml(m_manSectionIndexBuffer);
     QWebElement document = frame->documentElement();
     QWebElementCollection links = document.findAll("a");
     QMap<ManPage, QString> manMap;
     foreach(QWebElement e, links){
         ManPage page;
         if(e.hasAttribute("href") && !(e.attribute("href").contains(QRegExp( "#." )))){
             manMap.insert(qMakePair(e.toPlainText(), KUrl(e.attribute("href"))), sectionId);
         }
     }
     return m_manMap;
}

#include "manpagemodel.moc"

/*  This file is part of KDevelop

    Copyright 2010 Benjamin Port <port.benjamin@gmail.com>
    Copyright 2014 Kevin Funk <kfunk@kde.org>
    Copyright 2016 Andreas Cord-Landwehr <cordlandwehr@kde.org>

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

#include "qthelpconfig.h"

#include <QHelpEngineCore>
#include <QFileDialog>
#include <QToolButton>
#include <QHeaderView>

#include <KMessageBox>
#include <KMessageWidget>
#include <KLocalizedString>
#include <KNS3/Button>

#include "ui_qthelpconfig.h"
#include "ui_qthelpconfigeditdialog.h"
#include "qthelp_config_shared.h"
#include "debug.h"
#include "qthelpplugin.h"

enum Column
{
    NameColumn,
    PathColumn,
    IconColumn,
    GhnsColumn,
    ConfigColumn
};

class QtHelpConfigEditDialog : public QDialog, public Ui_QtHelpConfigEditDialog
{
public:
    explicit QtHelpConfigEditDialog(QTreeWidgetItem* modifiedItem, QtHelpConfig* parent = nullptr,
                                    Qt::WindowFlags f = nullptr)
        : QDialog(parent, f)
        , m_modifiedItem(modifiedItem)
        , m_config(parent)
    {
        setupUi(this);

        if (modifiedItem) {
            setWindowTitle(i18n("Modify Entry"));
        } else {
            setWindowTitle(i18n("Add New Entry"));
        }
        qchIcon->setIcon("qtlogo");
    }

    bool checkQtHelpFile();

    void accept() override;

private:
    QTreeWidgetItem* m_modifiedItem;
    QtHelpConfig* m_config;
};

bool QtHelpConfigEditDialog::checkQtHelpFile()
{
    //verify if the file is valid and if there is a name
    if(qchName->text().isEmpty()){
        KMessageBox::error(this, i18n("Name cannot be empty."));
        return false;
    }

    return m_config->checkNamespace(qchRequester->text(), m_modifiedItem);
}

void QtHelpConfigEditDialog::accept()
{
    if (!checkQtHelpFile())
        return;

    QDialog::accept();
}

QtHelpConfig::QtHelpConfig(QtHelpPlugin* plugin, QWidget *parent)
    : KDevelop::ConfigPage(plugin, nullptr, parent)
{
    m_configWidget = new Ui::QtHelpConfigUI;
    m_configWidget->setupUi(this);
    m_configWidget->addButton->setIcon(QIcon::fromTheme("list-add"));
    connect(m_configWidget->addButton, &QPushButton::clicked, this, &QtHelpConfig::add);

    // Table
    m_configWidget->qchTable->setColumnHidden(IconColumn, true);
    m_configWidget->qchTable->setColumnHidden(GhnsColumn, true);
    m_configWidget->qchTable->model()->setHeaderData(ConfigColumn, Qt::Horizontal, QVariant());
    m_configWidget->qchTable->header()->setSectionsMovable(false);
    m_configWidget->qchTable->header()->setStretchLastSection(false);
    m_configWidget->qchTable->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_configWidget->qchTable->header()->setSectionResizeMode(PathColumn, QHeaderView::Stretch);
    m_configWidget->qchTable->header()->setSectionResizeMode(ConfigColumn, QHeaderView::Fixed);

    // Add GHNS button
    KNS3::Button *knsButton = new KNS3::Button(i18nc("Allow user to get some API documentation with GHNS", "Get New Documentation"), "kdevelop-qthelp.knsrc", m_configWidget->boxQchManage);
    m_configWidget->tableCtrlLayout->insertWidget(1, knsButton);
    connect(knsButton, &KNS3::Button::dialogFinished, this, &QtHelpConfig::knsUpdate);
    connect(m_configWidget->loadQtDocsCheckBox, &QCheckBox::toggled,
            this, static_cast<void(QtHelpConfig::*)()>(&QtHelpConfig::changed));
    m_configWidget->qchSearchDir->setMode(KFile::Directory);
    connect(m_configWidget->qchSearchDir, &KUrlRequester::textChanged,
            this, &QtHelpConfig::changed);

    // Set availability information for QtHelp
    m_configWidget->messageAvailabilityQtDocs->setCloseButtonVisible(false);
    if(plugin->isQtHelpAvailable()) {
        m_configWidget->messageAvailabilityQtDocs->setVisible(false);
    } else {
        m_configWidget->messageAvailabilityQtDocs->setText(
            i18n("The command \"qmake -query\" could not provide a path to a QtHelp file (QCH)."));
        m_configWidget->loadQtDocsCheckBox->setVisible(false);
    }
    reset();
}

QtHelpConfig::~QtHelpConfig()
{
    delete m_configWidget;
}

KDevelop::ConfigPage::ConfigPageType QtHelpConfig::configPageType() const
{
    return KDevelop::ConfigPage::DocumentationConfigPage;
}

void QtHelpConfig::apply()
{
    QStringList iconList, nameList, pathList, ghnsList;
    for (int i = 0; i < m_configWidget->qchTable->topLevelItemCount(); i++) {
        const QTreeWidgetItem* item = m_configWidget->qchTable->topLevelItem(i);
        nameList << item->text(0);
        pathList << item->text(1);
        iconList << item->text(2);
        ghnsList << item->text(3);
    }
    QString searchDir = m_configWidget->qchSearchDir->text();
    bool loadQtDoc = m_configWidget->loadQtDocsCheckBox->isChecked();

    qtHelpWriteConfig(iconList, nameList, pathList, ghnsList, searchDir, loadQtDoc);
    static_cast<QtHelpPlugin*>(plugin())->readConfig();
}

void QtHelpConfig::reset()
{
    m_configWidget->qchTable->clear();

    QStringList iconList, nameList, pathList, ghnsList;
    QString searchDir;
    bool loadQtDoc;
    qtHelpReadConfig(iconList, nameList, pathList, ghnsList, searchDir, loadQtDoc);

    const int size = qMin(qMin(iconList.size(), nameList.size()), pathList.size());
    for(int i = 0; i < size; ++i) {
        QString ghnsStatus = ghnsList.size()>i ? ghnsList.at(i) : "0";
        addTableItem(iconList.at(i), nameList.at(i), pathList.at(i), ghnsStatus);
    }
    m_configWidget->qchSearchDir->setText(searchDir);
    m_configWidget->loadQtDocsCheckBox->setChecked(loadQtDoc);

    emit changed();
}

void QtHelpConfig::defaults()
{
    bool change = false;
    if(m_configWidget->qchTable->topLevelItemCount() > 0) {
        m_configWidget->qchTable->clear();
        change = true;
    }
    if(!m_configWidget->loadQtDocsCheckBox->isChecked()){
        m_configWidget->loadQtDocsCheckBox->setChecked(true);
        change = true;
    }

    if (change) {
        emit changed();
    }
}

void QtHelpConfig::add()
{
    QtHelpConfigEditDialog dialog(nullptr, this);
    if (!dialog.exec())
        return;

    QTreeWidgetItem* item = addTableItem(dialog.qchIcon->icon(), dialog.qchName->text(), dialog.qchRequester->text(), "0");
    m_configWidget->qchTable->setCurrentItem(item);
    emit changed();
}

void QtHelpConfig::modify(QTreeWidgetItem* item)
{
    if (!item)
        return;

    QtHelpConfigEditDialog dialog(item, this);
    if (item->text(GhnsColumn) != "0") {
        dialog.qchRequester->setText(i18n("Documentation provided by GHNS"));
        dialog.qchRequester->setEnabled(false);
    } else {
        dialog.qchRequester->setText(item->text(PathColumn));
        dialog.qchRequester->setEnabled(true);
    }
    dialog.qchName->setText(item->text(NameColumn));
    dialog.qchIcon->setIcon(item->text(IconColumn));
    if (!dialog.exec()) {
        return;
    }

    item->setIcon(NameColumn, QIcon(dialog.qchIcon->icon()));
    item->setText(NameColumn, dialog.qchName->text());
    item->setText(IconColumn, dialog.qchIcon->icon());
    if(item->text(GhnsColumn) == "0") {
        item->setText(PathColumn, dialog.qchRequester->text());
    }
    emit changed();
}

bool QtHelpConfig::checkNamespace(const QString& filename, QTreeWidgetItem* modifiedItem)
{
    QString qtHelpNamespace = QHelpEngineCore::namespaceName(filename);
    if (qtHelpNamespace.isEmpty()) {
        // Open error message (not valid Qt Compressed Help file)
        KMessageBox::error(this, i18n("Qt Compressed Help file is not valid."));
        return false;
    }
    // verify if it's the namespace it's not already in the list
    for(int i=0; i < m_configWidget->qchTable->topLevelItemCount(); i++) {
        const QTreeWidgetItem* item = m_configWidget->qchTable->topLevelItem(i);
        if (item != modifiedItem){
            if (qtHelpNamespace == QHelpEngineCore::namespaceName(item->text(PathColumn))) {
                // Open error message, documentation already imported
                KMessageBox::error(this, i18n("Documentation already imported"));
                return false;
            }
        }
    }
    return true;
}

void QtHelpConfig::remove(QTreeWidgetItem* item)
{
    if (!item)
        return;

    delete item;
    emit changed();
}

void QtHelpConfig::knsUpdate(KNS3::Entry::List list)
{
    if (list.isEmpty())
        return;

    foreach (const KNS3::Entry& e, list) {
        if(e.status() == KNS3::Entry::Installed) {
            if(e.installedFiles().size() == 1) {
                QString filename = e.installedFiles().at(0);
                if(checkNamespace(filename, nullptr)){
                    QTreeWidgetItem* item = addTableItem("documentation", e.name(), filename, "1");
                    m_configWidget->qchTable->setCurrentItem(item);
                } else {
                    qCDebug(QTHELP) << "namespace error";
                }
            }
        } else if(e.status() ==  KNS3::Entry::Deleted) {
            if(e.uninstalledFiles().size() == 1) {
                for(int i=0; i < m_configWidget->qchTable->topLevelItemCount(); i++) {
                    QTreeWidgetItem* item = m_configWidget->qchTable->topLevelItem(i);
                    if (e.uninstalledFiles().at(0) == item->text(PathColumn)) {
                        delete item;
                        break;
                    }
                }
            }
        }
    }
    emit changed();
}

QString QtHelpConfig::name() const
{
    return i18n("Qt Help");
}

QString QtHelpConfig::fullName() const
{
    return i18n("Configure Qt Help Settings");
}

QIcon QtHelpConfig::icon() const
{
    return QIcon::fromTheme(QStringLiteral("qtlogo"));
}

QTreeWidgetItem * QtHelpConfig::addTableItem(const QString &icon, const QString &name,
                                             const QString &path, const QString &ghnsStatus)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(m_configWidget->qchTable);
    item->setIcon(NameColumn, QIcon::fromTheme(icon));
    item->setText(NameColumn, name);
    item->setToolTip(NameColumn, name);
    item->setText(PathColumn, path);
    item->setToolTip(PathColumn, path);
    item->setText(IconColumn, icon);
    item->setText(GhnsColumn, ghnsStatus);

    QWidget *ctrlWidget = new QWidget(item->treeWidget());
    ctrlWidget->setLayout(new QHBoxLayout(ctrlWidget));

    QToolButton *modifyBtn = new QToolButton(item->treeWidget());
    modifyBtn->setIcon(QIcon::fromTheme("document-edit"));
    modifyBtn->setToolTip(i18n("Modify"));
    connect(modifyBtn, &QPushButton::clicked, this, [=](){
        modify(item);
    });
    QToolButton *removeBtn = new QToolButton(item->treeWidget());
    removeBtn->setIcon(QIcon::fromTheme("entry-delete"));
    removeBtn->setToolTip(i18n("Delete"));
    if (item->text(GhnsColumn) != "0") {
        // KNS3 currently does not provide API to uninstall entries
        // just removing the files results in wrong installed states in the KNS3 dialog
        // TODO: add API to KNS to remove files without UI interaction
        removeBtn->setEnabled(false);
        removeBtn->setToolTip(tr("Please uninstall this via GHNS"));
    } else {
        connect(removeBtn, &QPushButton::clicked, this, [=](){
            remove(item);
        });
    }
    ctrlWidget->layout()->addWidget(modifyBtn);
    ctrlWidget->layout()->addWidget(removeBtn);
    m_configWidget->qchTable->setItemWidget(item, ConfigColumn, ctrlWidget);

    return item;
}

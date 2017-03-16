/*  This file is part of KDevelop

    Copyright 2010 Benjamin Port <port.benjamin@gmail.com>
    Copyright 2014 Kevin Funk <kfunk@kde.org>

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

#ifndef QTHELPCONFIG_H
#define QTHELPCONFIG_H

#include <kdevplatform/interfaces/configpage.h>

#include <KNS3/Entry>

class QTreeWidgetItem;
class QtHelpPlugin;

namespace Ui
{
    class QtHelpConfigUI;
}

class QtHelpConfig : public KDevelop::ConfigPage
{
public:
    Q_OBJECT

    public:
      explicit QtHelpConfig(QtHelpPlugin* plugin, QWidget *parent = nullptr);
      ~QtHelpConfig() override;

      KDevelop::ConfigPage::ConfigPageType configPageType() const override;

      bool checkNamespace(const QString &filename, QTreeWidgetItem* modifiedItem);

      QString name() const override;
      QString fullName() const override;
      QIcon icon() const override;

    private slots:
      void add();
      void remove(QTreeWidgetItem* item);
      void modify(QTreeWidgetItem* item);
      void knsUpdate(KNS3::Entry::List list);

    public Q_SLOTS:
      void apply() override;
      void defaults() override;
      void reset() override;
    private:
      QTreeWidgetItem * addTableItem(const QString &icon, const QString &name,
                                     const QString &path, const QString &ghnsStatus);
      Ui::QtHelpConfigUI* m_configWidget;
};

#endif // QTHELPCONFIG_H

/***************************************************************************
 *   Copyright (C) 2003 by Alexander Dymo                                  *
 *   cloudtemple@mksat.net                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#include <klocale.h>
#include <kapplication.h>

#include <qcombobox.h>

#include "addfilesdialog.h"
#include "addfilesdialog.moc"

AddFilesDialog::AddFilesDialog(const QString& startDir, const QString& filter,
    QWidget *parent, const char *name, bool modal):
    KFileDialog(startDir, filter, parent, name, modal)
{
    KConfig *config = kapp->config();
    config->setGroup("Add Files Dialog");

    m_extraWidget = new QComboBox(false, this);
    m_extraWidget->insertItem(i18n("Copy File(s)"), 0);
    m_extraWidget->insertItem(i18n("Create Symbolic Link(s)"), 1);
    m_extraWidget->insertItem(i18n("Add Relative Path(s)"), 2);
    m_extraWidget->setCurrentItem(config->readNumEntry("Mode"));
    connect(m_extraWidget, SIGNAL(activated(int)), this, SLOT(storePreferred(int)));

    setPreviewWidget(m_extraWidget);

    setOperationMode(Opening);
}

#if KDE_VERSION >= 310
AddFilesDialog::AddFilesDialog(const QString& startDir, const QString& filter,
    QWidget *parent, const char *name, bool modal, QComboBox *extraWidget):
    KFileDialog(startDir, filter, parent, name, modal, extraWidget), m_extraWidget(extraWidget)
{
    KConfig *config = kapp->config();
    config->setGroup("Add Files Dialog");

    m_extraWidget->insertItem(i18n("Copy File(s)"), 0);
    m_extraWidget->insertItem(i18n("Create Symbolic Link(s)"), 1);
    m_extraWidget->insertItem(i18n("Add Relative Path(s)"), 2);
    m_extraWidget->setCurrentItem(config->readNumEntry("Mode"));
    connect(m_extraWidget, SIGNAL(activated(int)), this, SLOT(storePreferred(int)));

    setOperationMode(Opening);
}
#endif

AddFilesDialog::~AddFilesDialog()
{
}

AddFilesDialog::Mode AddFilesDialog::mode( )
{
    return (AddFilesDialog::Mode)m_extraWidget->currentItem();
}

void AddFilesDialog::storePreferred( int index )
{
    KConfig *config = kapp->config();
    config->setGroup("Add Files Dialog");
    config->writeEntry("Mode", index);
}

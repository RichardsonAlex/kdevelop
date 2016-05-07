/*
 * Displays registers.
 * Copyright 2013  Vlas Puhov <vlas.puhov@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "registersview.h"

#include "converters.h"
#include "debuglog.h"
#include "modelsmanager.h"

#include <QMenu>
#include <QContextMenuEvent>
#include <QSignalMapper>

#include <KLocalizedString>

using namespace KDevDebugger::GDB;

namespace
{
const int TABLES_COUNT = 5;
}

RegistersView::RegistersView(QWidget* p)
    : QWidget(p), m_menu(new QMenu(this)), m_mapper(new QSignalMapper(this)), m_modelsManager(0)
{
    setupUi(this);

    setupActions();

    connect(m_mapper, static_cast<void(QSignalMapper::*)(const QString&)>(&QSignalMapper::mapped), this, &RegistersView::menuTriggered);

    connect(tabWidget, &QTabWidget::currentChanged, this, &RegistersView::updateRegisters);
}

void RegistersView::contextMenuEvent(QContextMenuEvent* e)
{
    //Don't remove update action.
    const QList<QAction*> actions = m_menu->actions();
    for (int i = 1; i < actions.count(); i++) {
        m_menu->removeAction(actions[i]);
    }

    QString group = activeViews().first();

    foreach (QAction * act, m_actions) {
        act->setChecked(false);
    }

    const QVector<Format> formats = m_modelsManager->formats(group);
    if (formats.size() > 1) {
        QMenu* m = m_menu->addMenu(i18n("Format"));
        foreach (Format fmt, formats) {
            m->addAction(findAction(Converters::formatToString(fmt)));
        }
        findAction(Converters::formatToString(formats.first()))->setChecked(true);
    }

    const QVector<Mode> modes = m_modelsManager->modes(group);
    if (modes.size() > 1) {
        QMenu* m = m_menu->addMenu(i18n("Mode"));
        foreach (Mode mode, modes) {
            m->addAction(findAction(Converters::modeToString(mode)));
        }
        findAction(Converters::modeToString(modes.first()))->setChecked(true);
    }

    m_menu->exec(e->globalPos());
}

void RegistersView::updateRegisters()
{
    changeAvaliableActions();

    foreach (const QString & v, activeViews()) {
        m_modelsManager->updateRegisters(v);
    }
}

void RegistersView::menuTriggered(const QString& formatOrMode)
{
    Format f = Converters::stringToFormat(formatOrMode);
    if (f != LAST_FORMAT) {
        m_modelsManager->setFormat(activeViews().first(), f);
    } else {
        m_modelsManager->setMode(activeViews().first(), Converters::stringToMode(formatOrMode));
    }

    updateRegisters();
}

void RegistersView::changeAvaliableActions()
{
    const QString view = activeViews().first();
    if (view.isEmpty()) {
        return;
    }

    const QVector<Format> formats = m_modelsManager->formats(view) ;
    const QVector<Mode> modes = m_modelsManager->modes(view);

    foreach (QAction * a, m_actions) {
        bool enable = false;
        foreach (Format f, formats) {
            if (a->text() == Converters::formatToString(f)) {
                enable = true;
                break;
            }
        }

        if (!enable) {
            foreach (Mode m, modes) {
                if (a->text() == Converters::modeToString(m)) {
                    enable = true;
                    break;
                }
            }
        }

        a->setVisible(enable);
        a->setEnabled(enable);
    }
}

QAction* RegistersView::findAction(const QString& name)
{
    foreach (QAction * a, m_actions) {
        if (a->text() == name) {
            return a;
        }
    }
    return 0;
}

void RegistersView::addView(QTableView* view, int idx)
{
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    view->horizontalHeader()->hide();
    view->verticalHeader()->hide();
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setMinimumWidth(10);
    view->verticalHeader()->setDefaultSectionSize(15);

    QString name = m_modelsManager->addView(view);

    setNameForTable(idx, name);
}

void RegistersView::enable(bool enabled)
{
    setEnabled(enabled);
    if (enabled) {

        clear();

        addView(registers, 0);
        addView(flags, 0);
        addView(table_1, 1);
        addView(table_2, 2);
        addView(table_3, 3);

        changeAvaliableActions();
    }
}

void RegistersView::setNameForTable(int idx, const QString& name)
{
    qCDebug(DEBUGGERGDB) << name << " " << idx;
    const QString text = tabWidget->tabText(idx);
    if (!text.contains(name)) {
        tabWidget->setTabText(idx, text.isEmpty() ? name : text + '/' + name);
    }
}

void RegistersView::setModel(ModelsManager* m)
{
    m_modelsManager = m;
}

QStringList RegistersView::activeViews()
{
    return tabWidget->tabText(tabWidget->currentIndex()).split('/');
}

void RegistersView::clear()
{
    for (int i = 0; i < TABLES_COUNT; i++) {
        tabWidget->setTabText(i, "");
    }
}

void RegistersView::setupActions()
{
    QAction* updateAction = new QAction(this);
    updateAction->setShortcut(Qt::Key_U);
    updateAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    updateAction->setText(i18n("Update"));
    connect(updateAction, &QAction::triggered, this, &RegistersView::updateRegisters);
    addAction(updateAction);
    m_menu->addAction(updateAction);

    insertAction(Converters::formatToString(Binary), Qt::Key_B);
    insertAction(Converters::formatToString(Octal), Qt::Key_O);
    insertAction(Converters::formatToString(Decimal), Qt::Key_D);
    insertAction(Converters::formatToString(Hexadecimal), Qt::Key_H);
    insertAction(Converters::formatToString(Raw), Qt::Key_R);
    insertAction(Converters::formatToString(Unsigned), Qt::Key_N);

    insertAction(Converters::modeToString(u32), Qt::Key_I);
    insertAction(Converters::modeToString(u64), Qt::Key_L);
    insertAction(Converters::modeToString(f32), Qt::Key_F);
    insertAction(Converters::modeToString(f64), Qt::Key_P);

    insertAction(Converters::modeToString(v2_double), Qt::Key_P);
    insertAction(Converters::modeToString(v2_int64), Qt::Key_L);
    insertAction(Converters::modeToString(v4_float), Qt::Key_F);
    insertAction(Converters::modeToString(v4_int32), Qt::Key_I);
}

void RegistersView::insertAction(const QString& name, Qt::Key k)
{
    QAction* a = new QAction(this);
    a->setCheckable(true);
    a->setShortcut(k);
    a->setText(name);
    a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actions.append(a);
    addAction(a);

    m_mapper->setMapping(a, a->text());
    connect(a, &QAction::triggered, m_mapper, static_cast<void(QSignalMapper::*)()>(&QSignalMapper::map));
}

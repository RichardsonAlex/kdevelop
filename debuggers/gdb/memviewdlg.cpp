/***************************************************************************
    begin                : Tue Oct 5 1999
    copyright            : (C) 1999 by John Birch
    email                : jbb@kdevelop.org
 **************************************************************************
 * Copyright 2006 Vladimir Prus <ghost@cs.msu.su>
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "memviewdlg.h"

#include "dbgglobal.h"
#include "debugsession.h"
#include "mi/micommand.h"

#include <interfaces/icore.h>
#include <interfaces/idebugcontroller.h>

#include <KLocalizedString>

#include <Okteta/ByteArrayColumnView>
#include <Okteta/ByteArrayModel>

#include <QAction>
#include <QContextMenuEvent>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QMenu>
#include <QPushButton>
#include <QToolBox>
#include <QVariant>
#include <QVBoxLayout>

#include <cctype>

using KDevMI::MI::CommandType;

namespace KDevMI
{
namespace GDB
{

/** Container for controls that select memory range.
     *
    The memory range selection is embedded into memory view widget,
    it's not a standalone dialog. However, we want to have easy way
    to hide/show all controls, so we group them in this class.
*/
class MemoryRangeSelector : public QWidget
{
    public:
        QLineEdit* startAddressLineEdit;
        QLineEdit* amountLineEdit;
        QPushButton* okButton;
        QPushButton* cancelButton;

    explicit MemoryRangeSelector(QWidget* parent)
    : QWidget(parent)
    {
        QVBoxLayout* l = new QVBoxLayout(this);

        // Form layout: labels + address field
        auto formLayout = new QFormLayout();
        l->addLayout(formLayout);

        startAddressLineEdit = new QLineEdit(this);
        formLayout->addRow(i18n("Start:"), startAddressLineEdit);

        amountLineEdit = new QLineEdit(this);
        formLayout->addRow(i18n("Amount:"), amountLineEdit);

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
        l->addWidget(buttonBox);

        okButton = buttonBox->button(QDialogButtonBox::Ok);
        cancelButton = buttonBox->button(QDialogButtonBox::Cancel);

        setLayout(l);

        connect(startAddressLineEdit, &QLineEdit::returnPressed, okButton, [this]() {
                okButton->animateClick();
        });

        connect(amountLineEdit, &QLineEdit::returnPressed, okButton, [this]() {
                okButton->animateClick();
        });
    }
};

MemoryView::MemoryView(QWidget* parent)
: QWidget(parent),
    // New memory view can be created only when debugger is active,
    // so don't set s_appNotStarted here.
    m_memViewView(nullptr),
    m_debuggerState(0)
{
    setWindowTitle(i18n("Memory view"));
    emit captionChanged(windowTitle());

    initWidget();

    if (isOk())
        slotEnableOrDisable();

    auto debugController = KDevelop::ICore::self()->debugController();
    Q_ASSERT(debugController);

    connect(debugController, &KDevelop::IDebugController::currentSessionChanged,
            this, &MemoryView::currentSessionChanged);
}

void MemoryView::currentSessionChanged(KDevelop::IDebugSession* s)
{
    DebugSession *session = qobject_cast<DebugSession*>(s);
    if (!session) return;

    connect(session, &DebugSession::debuggerStateChanged,
             this, &MemoryView::slotStateChanged);
}

void MemoryView::slotStateChanged(DBGStateFlags oldState, DBGStateFlags newState)
{
    Q_UNUSED(oldState);
    debuggerStateChanged(newState);
}

void MemoryView::initWidget()
{
    QVBoxLayout *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);

    m_memViewModel = new Okteta::ByteArrayModel(0, -1, this);
    m_memViewView = new Okteta::ByteArrayColumnView(this);
    m_memViewView->setByteArrayModel(m_memViewModel);

    m_memViewModel->setReadOnly(false);
    m_memViewView->setReadOnly(false);
    m_memViewView->setOverwriteMode(true);
    m_memViewView->setOverwriteOnly(true);
    m_memViewModel->setAutoDelete(false);

    m_memViewView->setValueCoding( Okteta::ByteArrayColumnView::HexadecimalCoding );
    m_memViewView->setNoOfGroupedBytes(4);
    m_memViewView->setByteSpacingWidth(2);
    m_memViewView->setGroupSpacingWidth(12);
    m_memViewView->setLayoutStyle(Okteta::AbstractByteArrayView::FullSizeLayoutStyle);


    m_memViewView->setShowsNonprinting(false);
    m_memViewView->setSubstituteChar('*');

    m_rangeSelector = new MemoryRangeSelector(this);
    l->addWidget(m_rangeSelector);

    connect(m_rangeSelector->okButton, &QPushButton::clicked,
            this, &MemoryView::slotChangeMemoryRange);

    connect(m_rangeSelector->cancelButton, &QPushButton::clicked,
            this, &MemoryView::slotHideRangeDialog);

    connect(m_rangeSelector->startAddressLineEdit,
            &QLineEdit::textChanged,
            this,
            &MemoryView::slotEnableOrDisable);

    connect(m_rangeSelector->amountLineEdit,
            &QLineEdit::textChanged,
            this,
            &MemoryView::slotEnableOrDisable);

    l->addWidget(m_memViewView);
}

void MemoryView::debuggerStateChanged(DBGStateFlags state)
{
    if (isOk())
    {
        m_debuggerState = state;
        slotEnableOrDisable();
    }
}


void MemoryView::slotHideRangeDialog()
{
    m_rangeSelector->hide();
}

void MemoryView::slotChangeMemoryRange()
{
    DebugSession *session = qobject_cast<DebugSession*>(
        KDevelop::ICore::self()->debugController()->currentSession());
    if (!session) return;

    QString amount = m_rangeSelector->amountLineEdit->text();
    if(amount.isEmpty())
        amount = QString("sizeof(%1)").arg(m_rangeSelector->startAddressLineEdit->text());

    session->addCommand(new MI::ExpressionValueCommand(amount, this, &MemoryView::sizeComputed));
}

void MemoryView::sizeComputed(const QString& size)
{
    DebugSession *session = qobject_cast<DebugSession*>(
        KDevelop::ICore::self()->debugController()->currentSession());
    if (!session) return;

    session->addCommand(MI::DataReadMemory,
            QString("%1 x 1 1 %2")
                .arg(m_rangeSelector->startAddressLineEdit->text())
                .arg(size),
            this,
            &MemoryView::memoryRead);
}

void MemoryView::memoryRead(const MI::ResultRecord& r)
{
    const MI::Value& content = r["memory"][0]["data"];
    bool startStringConverted;
    m_memStart = r["addr"].literal().toULongLong(&startStringConverted, 16);
    m_memData.resize(content.size());

    m_memStartStr = m_rangeSelector->startAddressLineEdit->text();
    m_memAmountStr = m_rangeSelector->amountLineEdit->text();

    setWindowTitle(i18np("%2 (1 byte)","%2 (%1 bytes)",m_memData.size(),m_memStartStr));
    emit captionChanged(windowTitle());

    for(int i = 0; i < content.size(); ++i)
    {
        m_memData[i] = content[i].literal().toInt(0, 16);
    }

    m_memViewModel->setData(reinterpret_cast<Okteta::Byte*>(m_memData.data()), m_memData.size());

    slotHideRangeDialog();
}


void MemoryView::memoryEdited(int start, int end)
{
    DebugSession *session = qobject_cast<DebugSession*>(
        KDevelop::ICore::self()->debugController()->currentSession());
    if (!session) return;

    for(int i = start; i <= end; ++i)
    {
        session->addCommand(MI::GdbSet,
                QString("*(char*)(%1 + %2) = %3")
                    .arg(m_memStart)
                    .arg(i)
                    .arg(QString::number(m_memData[i])));
    }
}

void MemoryView::contextMenuEvent(QContextMenuEvent *e)
{
    if (!isOk())
        return;

    QMenu menu;

    bool app_running = !(m_debuggerState & s_appNotStarted);

    QAction* reload = menu.addAction(i18n("&Reload"));
    reload->setIcon(QIcon::fromTheme("view-refresh"));
    reload->setEnabled(app_running && !m_memData.isEmpty() );

    QActionGroup *formatGroup = NULL;
    QActionGroup *groupingGroup = NULL;
    if (m_memViewModel && m_memViewView)
    {
        // make Format menu with action group
        QMenu *formatMenu = new QMenu(i18n("&Format"));
        formatGroup = new QActionGroup(formatMenu);

        QAction *binary = formatGroup->addAction(i18n("&Binary"));
        binary->setData(Okteta::ByteArrayColumnView::BinaryCoding);
        binary->setShortcut(Qt::Key_B);
        formatMenu->addAction(binary);

        QAction *octal = formatGroup->addAction(i18n("&Octal"));
        octal->setData(Okteta::ByteArrayColumnView::OctalCoding);
        octal->setShortcut(Qt::Key_O);
        formatMenu->addAction(octal);

        QAction *decimal = formatGroup->addAction(i18n("&Decimal"));
        decimal->setData(Okteta::ByteArrayColumnView::DecimalCoding);
        decimal->setShortcut(Qt::Key_D);
        formatMenu->addAction(decimal);

        QAction *hex = formatGroup->addAction(i18n("&Hexadecimal"));
        hex->setData(Okteta::ByteArrayColumnView::HexadecimalCoding);
        hex->setShortcut(Qt::Key_H);
        formatMenu->addAction(hex);

        foreach(QAction* act, formatGroup->actions())
        {
            act->setCheckable(true);
            act->setChecked(act->data().toInt() ==  m_memViewView->valueCoding());
            act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        }

        menu.addMenu(formatMenu);


        // make Grouping menu with action group
        QMenu *groupingMenu = new QMenu(i18n("&Grouping"));
        groupingGroup = new QActionGroup(groupingMenu);

        QAction *group0 = groupingGroup->addAction(i18n("&0"));
        group0->setData(0);
        group0->setShortcut(Qt::Key_0);
        groupingMenu->addAction(group0);

        QAction *group1 = groupingGroup->addAction(i18n("&1"));
        group1->setData(1);
        group1->setShortcut(Qt::Key_1);
        groupingMenu->addAction(group1);

        QAction *group2 = groupingGroup->addAction(i18n("&2"));
        group2->setData(2);
        group2->setShortcut(Qt::Key_2);
        groupingMenu->addAction(group2);

        QAction *group4 = groupingGroup->addAction(i18n("&4"));
        group4->setData(4);
        group4->setShortcut(Qt::Key_4);
        groupingMenu->addAction(group4);

        QAction *group8 = groupingGroup->addAction(i18n("&8"));
        group8->setData(8);
        group8->setShortcut(Qt::Key_8);
        groupingMenu->addAction(group8);

        QAction *group16 = groupingGroup->addAction(i18n("1&6"));
        group16->setData(16);
        group16->setShortcut(Qt::Key_6);
        groupingMenu->addAction(group16);

        foreach(QAction* act, groupingGroup->actions())
        {
            act->setCheckable(true);
            act->setChecked(act->data().toInt() == m_memViewView->noOfGroupedBytes());
            act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        }

        menu.addMenu(groupingMenu);
    }

    QAction* write = menu.addAction(i18n("Write changes"));
    write->setIcon(QIcon::fromTheme("document-save"));
    write->setEnabled(app_running && m_memViewView && m_memViewView->isModified());

    QAction* range = menu.addAction(i18n("Change memory range"));
    range->setEnabled(app_running && !m_rangeSelector->isVisible());
    range->setIcon(QIcon::fromTheme("document-edit"));

    QAction* close = menu.addAction(i18n("Close this view"));
    close->setIcon(QIcon::fromTheme("window-close"));


    QAction* result = menu.exec(e->globalPos());


    if (result == reload)
    {
        // We use m_memStart and m_memAmount stored in this,
        // not textual m_memStartStr and m_memAmountStr,
        // because program position might have changes and expressions
        // are no longer valid.
        DebugSession *session = qobject_cast<DebugSession*>(
            KDevelop::ICore::self()->debugController()->currentSession());
        if (session) {
            session->addCommand(MI::DataReadMemory,
                    QString("%1 x 1 1 %2").arg(m_memStart).arg(m_memData.size()),
                    this,
                    &MemoryView::memoryRead);
        }
    }

    if (result && formatGroup && formatGroup == result->actionGroup())
        m_memViewView->setValueCoding( (Okteta::ByteArrayColumnView::ValueCoding)result->data().toInt());

    if (result && groupingGroup && groupingGroup == result->actionGroup())
        m_memViewView->setNoOfGroupedBytes(result->data().toInt());

    if (result == write)
    {
        memoryEdited(0, m_memData.size());
        m_memViewView->setModified(false);
    }

    if (result == range)
    {
        m_rangeSelector->startAddressLineEdit->setText(m_memStartStr);
        m_rangeSelector->amountLineEdit->setText(m_memAmountStr);

        m_rangeSelector->show();
        m_rangeSelector->startAddressLineEdit->setFocus();
    }

    if (result == close)
        delete this;
}

bool MemoryView::isOk() const
{
    return m_memViewView;
}

void MemoryView::slotEnableOrDisable()
{
    bool app_started = !(m_debuggerState & s_appNotStarted);

    bool enabled_ = app_started && !m_rangeSelector->startAddressLineEdit->text().isEmpty();

    m_rangeSelector->okButton->setEnabled(enabled_);
}


MemoryViewerWidget::MemoryViewerWidget(CppDebuggerPlugin* /*plugin*/, QWidget* parent)
: QWidget(parent)
{
    setWindowIcon(QIcon::fromTheme("server-database", windowIcon()));
    setWindowTitle(i18n("Memory viewer"));

    QAction * newMemoryViewerAction = new QAction(this);
    newMemoryViewerAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    newMemoryViewerAction->setText(i18n("New memory viewer"));
    newMemoryViewerAction->setToolTip(i18nc("@info:tooltip", "Open a new memory viewer."));
    newMemoryViewerAction->setIcon(QIcon::fromTheme("window-new"));
    connect(newMemoryViewerAction, &QAction::triggered, this , &MemoryViewerWidget::slotAddMemoryView);
    addAction(newMemoryViewerAction);

    QVBoxLayout *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);

    m_toolBox = new QToolBox(this);
    m_toolBox->setContentsMargins(0, 0, 0, 0);
    l->addWidget(m_toolBox);

    setLayout(l);

    // Start with one empty memory view.
    slotAddMemoryView();
}

void MemoryViewerWidget::slotAddMemoryView()
{
    MemoryView* widget = new MemoryView(this);
    m_toolBox->addItem(widget, widget->windowTitle());
    m_toolBox->setCurrentIndex(m_toolBox->indexOf(widget));

    connect(widget, &MemoryView::captionChanged,
            this, &MemoryViewerWidget::slotChildCaptionChanged);
}

void MemoryViewerWidget::slotChildCaptionChanged(const QString& caption)
{
    const QWidget* s = static_cast<const QWidget*>(sender());
    QWidget* ncs = const_cast<QWidget*>(s);
    QString cap = caption;
    // Prevent intepreting '&' as accelerator specifier.
    cap.replace('&', "&&");
    m_toolBox->setItemText(m_toolBox->indexOf(ncs), cap);
}

} // end of namespace GDB
} // end of namespace KDevMI

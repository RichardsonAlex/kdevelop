/*
 * MI based debugger specific Variable
 *
 * Copyright 2009 Vladimir Prus <ghost@cs.msu.su>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "mivariable.h"

#include "debuglog.h"
#include "midebugsession.h"
#include "mi/micommand.h"
#include "stringhelpers.h"

#include <debugger/interfaces/ivariablecontroller.h>
#include <interfaces/icore.h>

using namespace KDevelop;
using namespace KDevMI;
using namespace KDevMI::MI;

bool MIVariable::sessionIsAlive() const
{
    if (!debugSession)
        return false;

    IDebugSession::DebuggerState s = debugSession->state();
    return s != IDebugSession::NotStartedState 
        && s != IDebugSession::EndedState
        && !debugSession->debuggerStateIsOn(s_shuttingDown);
}

MIVariable::MIVariable(MIDebugSession *session, TreeModel* model, TreeItem* parent,
                       const QString& expression, const QString& display)
    : Variable(model, parent, expression, display)
    , debugSession(session)
{
}

MIVariable *MIVariable::createChild(const Value& child)
{
    if (!debugSession) return nullptr;
    auto var = static_cast<MIVariable*>(debugSession->variableController()->createVariable(model(), this, child["exp"].literal()));
    var->setTopLevel(false);
    var->setVarobj(child["name"].literal());
    bool hasMore = child["numchild"].toInt() != 0 || ( child.hasField("dynamic") && child["dynamic"].toInt()!=0 );
    var->setHasMoreInitial(hasMore);

    // *this must be parent's child before we can set type and value
    appendChild(var);

    var->setType(child["type"].literal());
    var->setValue(formatValue(child["value"].literal()));
    var->setChanged(true);
    return var;
}

MIVariable::~MIVariable()
{
    if (!varobj_.isEmpty())
    {
        // Delete only top-level variable objects.
        if (topLevel()) {
            if (sessionIsAlive()) {
                debugSession->addCommand(VarDelete, QString("\"%1\"").arg(varobj_));
            }
        }
        if (debugSession)
            debugSession->variableMapping().remove(varobj_);
    }
}

void MIVariable::setVarobj(const QString& v)
{
    if (!debugSession) {
        qCWarning(DEBUGGERCOMMON) << "MIVariable::setVarobj called when its session died";
        return;
    }
    if (!varobj_.isEmpty()) {
        // this should not happen
        // but apperently it does when attachMaybe is called a second time before
        // the first -var-create call returned
        debugSession->variableMapping().remove(varobj_);
    }
    varobj_ = v;
    debugSession->variableMapping()[varobj_] = this;
}


static int nextId = 0;

class CreateVarobjHandler : public MICommandHandler
{
public:
    CreateVarobjHandler(MIVariable *variable, QObject *callback, const char *callbackMethod)
    : m_variable(variable), m_callback(callback), m_callbackMethod(callbackMethod)
    {}

    void handle(const ResultRecord &r) override
    {
        if (!m_variable) return;
        bool hasValue = false;
        MIVariable* variable = m_variable.data();
        variable->deleteChildren();
        variable->setInScope(true);
        if (r.reason == "error") {
            variable->setShowError(true);
        } else {
            variable->setVarobj(r["name"].literal());

            bool hasMore = false;
            if (r.hasField("has_more") && r["has_more"].toInt())
                // GDB swears there are more children. Trust it
                hasMore = true;
            else
                // There are no more children in addition to what
                // numchild reports. But, in KDevelop, the variable
                // is not yet expanded, and those numchild are not
                // fetched yet. So, if numchild != 0, hasMore should
                // be true.
                hasMore = r["numchild"].toInt() != 0;

            variable->setHasMore(hasMore);

            variable->setType(r["type"].literal());
            variable->setValue(variable->formatValue(r["value"].literal()));
            hasValue = !r["value"].literal().isEmpty();
            if (variable->isExpanded() && r["numchild"].toInt()) {
                variable->fetchMoreChildren();
            }

            if (variable->format() != KDevelop::Variable::Natural) {
                //TODO doesn't work for children as they are not yet loaded
                variable->formatChanged();
            }
        }

        if (m_callback && m_callbackMethod) {
            QMetaObject::invokeMethod(m_callback, m_callbackMethod, Q_ARG(bool, hasValue));
        }
    }
    bool handlesError() override { return true; }

private:
    QPointer<MIVariable> m_variable;
    QObject *m_callback;
    const char *m_callbackMethod;
};

void MIVariable::attachMaybe(QObject *callback, const char *callbackMethod)
{
    if (!varobj_.isEmpty())
        return;

    // Try find a current session and attach to it
    if (!ICore::self()->debugController()) return; //happens on shutdown
    debugSession = static_cast<MIDebugSession*>(ICore::self()->debugController()->currentSession());

    if (sessionIsAlive()) {
        debugSession->addCommand(VarCreate,
                                 QString("var%1 @ %2").arg(nextId++).arg(enquotedExpression()),
                                 new CreateVarobjHandler(this, callback, callbackMethod));
    }
}

void MIVariable::markAsDead()
{
    varobj_.clear();
}

class FetchMoreChildrenHandler : public MICommandHandler
{
public:
    FetchMoreChildrenHandler(MIVariable *variable, MIDebugSession *session)
        : m_variable(variable), m_session(session), m_activeCommands(1)
    {}

    void handle(const ResultRecord &r) override
    {
        if (!m_variable) return;
        --m_activeCommands;

        MIVariable* variable = m_variable.data();

        if (r.hasField("children"))
        {
            const Value& children = r["children"];
            for (int i = 0; i < children.size(); ++i) {
                const Value& child = children[i];
                const QString& exp = child["exp"].literal();
                if (exp == "public" || exp == "protected" || exp == "private") {
                    ++m_activeCommands;
                    m_session->addCommand(VarListChildren,
                                          QString("--all-values \"%1\"").arg(child["name"].literal()),
                                          this/*use again as handler*/);
                } else {
                    variable->createChild(child);
                    // it's automatically appended to variable's children list
                }
            }
        }

        /* Note that we don't set hasMore to true if there are still active
           commands. The reason is that we don't want the user to have
           even theoretical ability to click on "..." item and confuse
           us.  */
        bool hasMore = false;
        if (r.hasField("has_more"))
            hasMore = r["has_more"].toInt();

        variable->setHasMore(hasMore);
        if (m_activeCommands == 0) {
            variable->emitAllChildrenFetched();
            delete this;
        }
    }
    bool handlesError() override {
        // FIXME: handle error?
        return false;
    }
    bool autoDelete() override {
        // we delete ourselve
        return false;
    }

private:
    QPointer<MIVariable> m_variable;
    MIDebugSession *m_session;
    int m_activeCommands;
};

void MIVariable::fetchMoreChildren()
{
    int c = childItems.size();
    // FIXME: should not even try this if app is not started.
    // Probably need to disable open, or something
    if (sessionIsAlive()) {
        debugSession->addCommand(VarListChildren,
                                 QString("--all-values \"%1\" %2 %3")
                                 //   fetch    from ..    to ..
                                 .arg(varobj_).arg(c).arg(c + fetchStep),
                                 new FetchMoreChildrenHandler(this, debugSession));
    }
}

void MIVariable::handleUpdate(const Value& var)
{
    if (var.hasField("type_changed")
        && var["type_changed"].literal() == "true")
    {
        deleteChildren();
        // FIXME: verify that this check is right.
        setHasMore(var["new_num_children"].toInt() != 0);
        fetchMoreChildren();
    }

    if (var.hasField("in_scope") && var["in_scope"].literal() == "false")
    {
        setInScope(false);
    }
    else
    {
        setInScope(true);

        if  (var.hasField("new_num_children")) {
            int nc = var["new_num_children"].toInt();
            Q_ASSERT(nc != -1);
            setHasMore(false);
            while (childCount() > nc) {
                TreeItem *c = child(childCount()-1);
                removeChild(childCount()-1);
                delete c;
            }
        }

        if (var.hasField("new_children"))
        {
            const Value& children = var["new_children"];
            if (debugSession) {
                for (int i = 0; i < children.size(); ++i) {
                    createChild(children[i]);
                    // it's automatically appended to this's children list
                }
            }
        }

        if (var.hasField("type_changed") && var["type_changed"].literal() == "true") {
            setType(var["new_type"].literal());
        }
        setValue(formatValue(var["value"].literal()));
        setChanged(true);
        setHasMore(var.hasField("has_more") && var["has_more"].toInt());
    }
}

const QString& MIVariable::varobj() const
{
    return varobj_;
}

QString MIVariable::enquotedExpression() const
{
    return Utils::quoteExpression(expression());
}


class SetFormatHandler : public MICommandHandler
{
public:
    explicit SetFormatHandler(MIVariable *var)
        : m_variable(var)
    {}

    void handle(const ResultRecord &r) override
    {
        if(m_variable && r.hasField("value"))
            m_variable->setValue(m_variable->formatValue(r["value"].literal()));
    }
private:
    QPointer<MIVariable> m_variable;
};

void MIVariable::formatChanged()
{
    if(childCount())
    {
        foreach(TreeItem* item, childItems) {
            Q_ASSERT(dynamic_cast<MIVariable*>(item));
            if( MIVariable* var=dynamic_cast<MIVariable*>(item))
                var->setFormat(format());
        }
    }
    else
    {
        if (sessionIsAlive()) {
            debugSession->addCommand(VarSetFormat,
                                     QString(" %1 %2 ").arg(varobj_).arg(format2str(format())),
                                     new SetFormatHandler(this));
        }
    }
}

QString MIVariable::formatValue(const QString &rawValue) const
{
    return rawValue;
}

/*
 * KDevelop xUnit testing support
 *
 * Copyright 2008 Manuel Breugelmans
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
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

#ifndef VERITASCPP_CONSTRUCTORSKELETON_INCLUDED
#define VERITASCPP_CONSTRUCTORSKELETON_INCLUDED

#include "veritascppexport.h"
#include <QStringList>
#include "methodskeleton.h"

namespace Veritas
{

/*! Value class which stores a simplified AST for constructors methods */
class VERITASCPP_EXPORT ConstructorSkeleton : public MethodSkeleton
{
public:
    ConstructorSkeleton();
    virtual ~ConstructorSkeleton();

    void addInitializer(const QString&);
    QStringList initializerList() const;

private:
    QStringList m_initializerList;
};

}

#endif // VERITASCPP_METHODSKELETON_INCLUDED

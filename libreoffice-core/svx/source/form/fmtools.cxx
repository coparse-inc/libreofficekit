/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */


#include <fmprop.hxx>
#include <fmservs.hxx>
#include <svx/fmtools.hxx>
#include <svx/svdobjkind.hxx>

#include <com/sun/star/awt/LineEndFormat.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/container/XIndexAccess.hpp>
#include <com/sun/star/io/XPersistObject.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/sdb/ErrorCondition.hpp>
#include <com/sun/star/sdb/ErrorMessageDialog.hpp>
#include <com/sun/star/sdb/SQLContext.hpp>
#include <com/sun/star/sdb/SQLErrorEvent.hpp>
#include <com/sun/star/sdb/XCompletedConnection.hpp>
#include <com/sun/star/sdb/XResultSetAccess.hpp>
#include <com/sun/star/sdbc/XRowSet.hpp>
#include <com/sun/star/ui/dialogs/XExecutableDialog.hpp>
#include <com/sun/star/util/Language.hpp>

#include <comphelper/processfactory.hxx>
#include <comphelper/property.hxx>
#include <comphelper/types.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <tools/debug.hxx>
#include <tools/diagnose_ex.h>
#include <vcl/svapp.hxx>

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::util;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::awt;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::ui::dialogs;
using namespace ::com::sun::star::sdbc;
using namespace ::com::sun::star::sdbcx;
using namespace ::com::sun::star::sdb;
using namespace ::com::sun::star::task;
using namespace ::com::sun::star::form;
using namespace ::svxform;


namespace
{
    bool lcl_shouldDisplayError( const Any& _rError )
    {
        SQLException aError;
        if ( !( _rError >>= aError ) )
            return true;

        if ( ! aError.Message.startsWith( "[OOoBase]" ) )
            // it is an exception *not* thrown by an OOo Base core component
            return true;

        // the only exception we do not display ATM is a RowSetVetoException, which
        // has been raised because an XRowSetApprovalListener vetoed a change
        if ( aError.ErrorCode + ErrorCondition::ROW_SET_OPERATION_VETOED == 0 )
            return false;

        // everything else is to be displayed
        return true;
    }
}

void displayException(const Any& _rExcept, const css::uno::Reference<css::awt::XWindow>& rParent)
{
    // check whether we need to display it
    if ( !lcl_shouldDisplayError( _rExcept ) )
        return;

    try
    {
        Reference< XExecutableDialog > xErrorDialog = ErrorMessageDialog::create(::comphelper::getProcessComponentContext(), "", rParent, _rExcept);
        xErrorDialog->execute();
    }
    catch(const Exception&)
    {
        TOOLS_WARN_EXCEPTION("svx.form", "could not display the error message!");
    }
}

void displayException(const css::sdbc::SQLException& _rExcept, const css::uno::Reference<css::awt::XWindow>& rParent)
{
    displayException(makeAny(_rExcept), rParent);
}

void displayException(const css::sdb::SQLContext& _rExcept, const css::uno::Reference<css::awt::XWindow>& rParent)
{
    displayException(makeAny(_rExcept), rParent);
}

void displayException(const css::sdb::SQLErrorEvent& _rEvent, const css::uno::Reference<css::awt::XWindow>& rParent)
{
    displayException(_rEvent.Reason, rParent);
}

sal_Int32 getElementPos(const Reference< css::container::XIndexAccess>& xCont, const Reference< XInterface >& xElement)
{
    sal_Int32 nIndex = -1;
    if (!xCont.is())
        return nIndex;


    Reference< XInterface > xNormalized( xElement, UNO_QUERY );
    DBG_ASSERT( xNormalized.is(), "getElementPos: invalid element!" );
    if ( xNormalized.is() )
    {
        // find child position
        nIndex = xCont->getCount();
        while (nIndex--)
        {
            try
            {
                Reference< XInterface > xCurrent(xCont->getByIndex( nIndex ),UNO_QUERY);
                DBG_ASSERT( xCurrent.get() == Reference< XInterface >( xCurrent, UNO_QUERY ).get(),
                    "getElementPos: container element not normalized!" );
                if ( xNormalized.get() == xCurrent.get() )
                    break;
            }
            catch(Exception&)
            {
                TOOLS_WARN_EXCEPTION( "svx", "getElementPos" );
            }

        }
    }
    return nIndex;
}


OUString getLabelName(const Reference< css::beans::XPropertySet>& xControlModel)
{
    if (!xControlModel.is())
        return OUString();

    if (::comphelper::hasProperty(FM_PROP_CONTROLLABEL, xControlModel))
    {
        Reference< css::beans::XPropertySet> xLabelSet;
        xControlModel->getPropertyValue(FM_PROP_CONTROLLABEL) >>= xLabelSet;
        if (xLabelSet.is() && ::comphelper::hasProperty(FM_PROP_LABEL, xLabelSet))
        {
            Any aLabel( xLabelSet->getPropertyValue(FM_PROP_LABEL) );
            if ((aLabel.getValueTypeClass() == TypeClass_STRING) && !::comphelper::getString(aLabel).isEmpty())
                return ::comphelper::getString(aLabel);
        }
    }

    return ::comphelper::getString(xControlModel->getPropertyValue(FM_PROP_CONTROLSOURCE));
}


// = CursorWrapper

CursorWrapper::CursorWrapper(const Reference< css::sdbc::XRowSet>& _rxCursor, bool bUseCloned)
{
    ImplConstruct(Reference< css::sdbc::XResultSet>(_rxCursor), bUseCloned);
}


CursorWrapper::CursorWrapper(const Reference< css::sdbc::XResultSet>& _rxCursor, bool bUseCloned)
{
    ImplConstruct(_rxCursor, bUseCloned);
}


void CursorWrapper::ImplConstruct(const Reference< css::sdbc::XResultSet>& _rxCursor, bool bUseCloned)
{
    if (bUseCloned)
    {
        Reference< css::sdb::XResultSetAccess> xAccess(_rxCursor, UNO_QUERY);
        try
        {
            m_xMoveOperations = xAccess.is() ? xAccess->createResultSet() : Reference< css::sdbc::XResultSet>();
        }
        catch(Exception&)
        {
        }
    }
    else
        m_xMoveOperations   = _rxCursor;

    m_xBookmarkOperations.set(m_xMoveOperations, css::uno::UNO_QUERY);
    m_xColumnsSupplier.set(m_xMoveOperations, css::uno::UNO_QUERY);
    m_xPropertyAccess.set(m_xMoveOperations, css::uno::UNO_QUERY);

    if ( !m_xMoveOperations.is() || !m_xBookmarkOperations.is() || !m_xColumnsSupplier.is() || !m_xPropertyAccess.is() )
    {   // all or nothing !!
        m_xMoveOperations = nullptr;
        m_xBookmarkOperations = nullptr;
        m_xColumnsSupplier = nullptr;
    }
    else
        m_xGeneric = m_xMoveOperations.get();
}

CursorWrapper& CursorWrapper::operator=(const Reference< css::sdbc::XRowSet>& _rxCursor)
{
    m_xMoveOperations.set(_rxCursor);
    m_xBookmarkOperations.set(_rxCursor, UNO_QUERY);
    m_xColumnsSupplier.set(_rxCursor, UNO_QUERY);
    if (!m_xMoveOperations.is() || !m_xBookmarkOperations.is() || !m_xColumnsSupplier.is())
    {   // all or nothing !!
        m_xMoveOperations = nullptr;
        m_xBookmarkOperations = nullptr;
        m_xColumnsSupplier = nullptr;
    }
    return *this;
}

FmXDisposeListener::~FmXDisposeListener()
{
    setAdapter(nullptr);
}

void FmXDisposeListener::setAdapter(FmXDisposeMultiplexer* pAdapter)
{
    std::scoped_lock aGuard(m_aMutex);
    m_pAdapter = pAdapter;
}

FmXDisposeMultiplexer::FmXDisposeMultiplexer(FmXDisposeListener* _pListener, const Reference< css::lang::XComponent>& _rxObject)
    :m_xObject(_rxObject)
    ,m_pListener(_pListener)
{
    m_pListener->setAdapter(this);

    if (m_xObject.is())
        m_xObject->addEventListener(this);
}

FmXDisposeMultiplexer::~FmXDisposeMultiplexer()
{
}

// css::lang::XEventListener

void FmXDisposeMultiplexer::disposing(const css::lang::EventObject& /*Source*/)
{
    Reference< css::lang::XEventListener> xPreventDelete(this);

    if (m_pListener)
    {
        m_pListener->disposing(0);
        m_pListener->setAdapter(nullptr);
        m_pListener = nullptr;
    }
    m_xObject = nullptr;
}


void FmXDisposeMultiplexer::dispose()
{
    if (m_xObject.is())
    {
        Reference< css::lang::XEventListener> xPreventDelete(this);

        m_xObject->removeEventListener(this);
        m_xObject = nullptr;

        m_pListener->setAdapter(nullptr);
        m_pListener = nullptr;
    }
}


sal_Int16 getControlTypeByObject(const Reference< css::lang::XServiceInfo>& _rxObject)
{
    // ask for the persistent service name
    Reference< css::io::XPersistObject> xPersistence(_rxObject, UNO_QUERY);
    DBG_ASSERT(xPersistence.is(), "::getControlTypeByObject : argument should be a css::io::XPersistObject !");
    if (!xPersistence.is())
        return OBJ_FM_CONTROL;

    OUString sPersistentServiceName = xPersistence->getServiceName();
    if (sPersistentServiceName == FM_COMPONENT_EDIT)   // 5.0-Name
    {
        // may be a simple edit field or a formatted field, dependent of the supported services
        if (_rxObject->supportsService(FM_SUN_COMPONENT_FORMATTEDFIELD))
            return OBJ_FM_FORMATTEDFIELD;
        return OBJ_FM_EDIT;
    }
    if (sPersistentServiceName == FM_COMPONENT_TEXTFIELD)
        return OBJ_FM_EDIT;
    if (sPersistentServiceName == FM_COMPONENT_COMMANDBUTTON)
        return OBJ_FM_BUTTON;
    if (sPersistentServiceName == FM_COMPONENT_FIXEDTEXT)
        return OBJ_FM_FIXEDTEXT;
    if (sPersistentServiceName == FM_COMPONENT_LISTBOX)
        return OBJ_FM_LISTBOX;
    if (sPersistentServiceName == FM_COMPONENT_CHECKBOX)
        return OBJ_FM_CHECKBOX;
    if (sPersistentServiceName == FM_COMPONENT_RADIOBUTTON)
        return OBJ_FM_RADIOBUTTON;
    if (sPersistentServiceName == FM_COMPONENT_GROUPBOX)
        return OBJ_FM_GROUPBOX;
    if (sPersistentServiceName == FM_COMPONENT_COMBOBOX)
        return OBJ_FM_COMBOBOX;
    if (sPersistentServiceName == FM_COMPONENT_GRID)   // 5.0-Name
        return OBJ_FM_GRID;
    if (sPersistentServiceName == FM_COMPONENT_GRIDCONTROL)
        return OBJ_FM_GRID;
    if (sPersistentServiceName == FM_COMPONENT_IMAGEBUTTON)
        return OBJ_FM_IMAGEBUTTON;
    if (sPersistentServiceName == FM_COMPONENT_FILECONTROL)
        return OBJ_FM_FILECONTROL;
    if (sPersistentServiceName == FM_COMPONENT_DATEFIELD)
        return OBJ_FM_DATEFIELD;
    if (sPersistentServiceName == FM_COMPONENT_TIMEFIELD)
        return OBJ_FM_TIMEFIELD;
    if (sPersistentServiceName == FM_COMPONENT_NUMERICFIELD)
        return OBJ_FM_NUMERICFIELD;
    if (sPersistentServiceName == FM_COMPONENT_CURRENCYFIELD)
        return OBJ_FM_CURRENCYFIELD;
    if (sPersistentServiceName == FM_COMPONENT_PATTERNFIELD)
        return OBJ_FM_PATTERNFIELD;
    if (sPersistentServiceName == FM_COMPONENT_HIDDEN) // 5.0-Name
        return OBJ_FM_HIDDEN;
    if (sPersistentServiceName == FM_COMPONENT_HIDDENCONTROL)
        return OBJ_FM_HIDDEN;
    if (sPersistentServiceName == FM_COMPONENT_IMAGECONTROL)
        return OBJ_FM_IMAGECONTROL;
    if (sPersistentServiceName == FM_COMPONENT_FORMATTEDFIELD)
    {
        OSL_FAIL("::getControlTypeByObject : suspicious persistent service name (formatted field) !");
            // objects with that service name should exist as they aren't compatible with older versions
        return OBJ_FM_FORMATTEDFIELD;
    }
    if ( sPersistentServiceName == FM_SUN_COMPONENT_SCROLLBAR )
        return OBJ_FM_SCROLLBAR;
    if ( sPersistentServiceName == FM_SUN_COMPONENT_SPINBUTTON )
        return OBJ_FM_SPINBUTTON;
    if ( sPersistentServiceName == FM_SUN_COMPONENT_NAVIGATIONBAR )
        return OBJ_FM_NAVIGATIONBAR;

    OSL_FAIL("::getControlTypeByObject : unknown object type !");
    return OBJ_FM_CONTROL;
}


bool isRowSetAlive(const Reference< XInterface >& _rxRowSet)
{
    bool bIsAlive = false;
    Reference< css::sdbcx::XColumnsSupplier> xSupplyCols(_rxRowSet, UNO_QUERY);
    Reference< css::container::XIndexAccess> xCols;
    if (xSupplyCols.is())
        xCols.set(xSupplyCols->getColumns(), UNO_QUERY);
    if (xCols.is() && (xCols->getCount() > 0))
        bIsAlive = true;

    return bIsAlive;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

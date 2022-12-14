/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*************************************************************************
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * Copyright 2000, 2010 Oracle and/or its affiliates.
 *
 * OpenOffice.org - a multi-platform office productivity suite
 *
 * This file is part of OpenOffice.org.
 *
 * OpenOffice.org is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * only, as published by the Free Software Foundation.
 *
 * OpenOffice.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 3 for more details
 * (a copy is included in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with OpenOffice.org.  If not, see
 * <http://www.openoffice.org/license.html>
 * for a copy of the LGPLv3 License.
 *
 ************************************************************************/

#include <memory>
#include "DAVSessionFactory.hxx"
#include "NeonSession.hxx"
#include "NeonUri.hxx"
#include <osl/diagnose.h>

using namespace webdav_ucp;
using namespace com::sun::star;

DAVSessionFactory::~DAVSessionFactory()
{
}

rtl::Reference< DAVSession > DAVSessionFactory::createDAVSession(
                const OUString & inUri,
                const uno::Sequence< beans::NamedValue >& rFlags,
                const uno::Reference< uno::XComponentContext > & rxContext )
{
    std::scoped_lock aGuard( m_aMutex );

    m_xContext = rxContext;

    if (!m_xProxyDecider)
        m_xProxyDecider.reset( new ucbhelper::InternetProxyDecider( rxContext ) );

    Map::iterator aIt = std::find_if(m_aMap.begin(), m_aMap.end(),
        [&inUri, &rFlags](const Map::value_type& rEntry) { return rEntry.second->CanUse( inUri, rFlags ); });

    if ( aIt == m_aMap.end() )
    {
        NeonUri aURI( inUri );

        std::unique_ptr<DAVSession> xElement(
            new NeonSession(this, inUri, rFlags, *m_xProxyDecider));

        aIt = m_aMap.emplace( inUri, xElement.get() ).first;
        aIt->second->m_aContainerIt = aIt;
        xElement.release();
        return aIt->second;
    }
    else if ( osl_atomic_increment( &aIt->second->m_nRefCount ) > 1 )
    {
        rtl::Reference< DAVSession > xElement( aIt->second );
        osl_atomic_decrement( &aIt->second->m_nRefCount );
        return xElement;
    }
    else
    {
        osl_atomic_decrement( &aIt->second->m_nRefCount );
        aIt->second->m_aContainerIt = m_aMap.end();

        // If URL scheme is different from http or https we definitely
        // have to use a proxy and therefore can optimize the getProxy
        // call a little:
        NeonUri aURI( inUri );

        aIt->second = new NeonSession(this, inUri, rFlags, *m_xProxyDecider);
        aIt->second->m_aContainerIt = aIt;
        return aIt->second;
    }
}

void DAVSessionFactory::releaseElement( DAVSession const * pElement )
{
    OSL_ASSERT( pElement );
    std::scoped_lock aGuard( m_aMutex );
    if ( pElement->m_aContainerIt != m_aMap.end() )
        m_aMap.erase( pElement->m_aContainerIt );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

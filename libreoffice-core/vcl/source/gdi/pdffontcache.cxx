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

#include <sal/config.h>

#include <sal/types.h>

#include <font/PhysicalFontFace.hxx>
#include <pdf/pdffontcache.hxx>
#include <salgdi.hxx>

#include <typeinfo>

using namespace vcl;

PDFFontCache::FontIdentifier::FontIdentifier( const vcl::font::PhysicalFontFace* pFont, bool bVertical ) :
    m_nFontId( pFont->GetFontId() ),
    m_bVertical( bVertical ),
    m_typeFontFace( const_cast<std::type_info*>(&typeid(pFont)) )
{
}

PDFFontCache::FontData& PDFFontCache::getFont( const vcl::font::PhysicalFontFace* pFont, bool bVertical )
{
    FontIdentifier aId( pFont, bVertical );
    FontToIndexMap::iterator it = m_aFontToIndex.find( aId );
    if( it != m_aFontToIndex.end() )
        return m_aFonts[ it->second ];
    m_aFontToIndex[ aId ] = sal_uInt32(m_aFonts.size());
    m_aFonts.emplace_back( );
    return m_aFonts.back();
}

sal_Int32 PDFFontCache::getGlyphWidth( const vcl::font::PhysicalFontFace* pFont, sal_GlyphId nGlyph, bool bVertical, SalGraphics* pGraphics )
{
    sal_Int32 nWidth = 0;
    FontData& rFontData( getFont( pFont, bVertical ) );
    if( rFontData.m_nWidths.empty() )
    {
        pGraphics->GetGlyphWidths( pFont, bVertical, rFontData.m_nWidths, rFontData.m_aGlyphIdToIndex );
    }
    if( ! rFontData.m_nWidths.empty() )
    {
        if (nGlyph < rFontData.m_nWidths.size())
            nWidth = rFontData.m_nWidths[nGlyph];
    }
    return nWidth;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

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

#pragma once

#include <sal/config.h>

#include <typeinfo>

#include <sal/types.h>

#include <salgdi.hxx>

namespace vcl
{
    class PDFFontCache
    {
        struct FontIdentifier
        {
            sal_IntPtr      m_nFontId;
            bool            m_bVertical;
            std::type_info* m_typeFontFace;

            FontIdentifier( const vcl::font::PhysicalFontFace*, bool bVertical );

            // Less than needed for std::set and std::map
            bool operator<( const FontIdentifier& rRight ) const
            {
                std::type_info *pType = rRight.m_typeFontFace;

                return m_nFontId < rRight.m_nFontId ||
                       ( m_nFontId == rRight.m_nFontId &&
                       ( m_typeFontFace->before( *pType ) ||
                       ( *m_typeFontFace == *pType && m_bVertical < rRight.m_bVertical ) ) );
            }
        };
        struct FontData
        {
            std::vector< sal_Int32 >  m_nWidths;
            Ucs2UIntMap  m_aGlyphIdToIndex;
        };
        typedef std::map< FontIdentifier, sal_uInt32 > FontToIndexMap;

        std::vector< FontData >     m_aFonts;
        FontToIndexMap              m_aFontToIndex;

        FontData& getFont( const vcl::font::PhysicalFontFace*, bool bVertical );
        public:
        PDFFontCache() {}

        sal_Int32 getGlyphWidth( const vcl::font::PhysicalFontFace*, sal_GlyphId, bool bVertical, SalGraphics* );
    };
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

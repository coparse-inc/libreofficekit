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


#include <com/sun/star/i18n/CharacterIteratorMode.hpp>
#include <o3tl/safeint.hxx>
#include <breakiterator_th.hxx>
#include <wtt.h>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::i18n;
using namespace ::com::sun::star::lang;

namespace i18npool {

/**
 * Constructor.
 */
BreakIterator_th::BreakIterator_th()
{
    cBreakIterator = "com.sun.star.i18n.BreakIterator_th";
    // to improve performance, alloc big enough memory in construct.
    m_aNextCellIndex.assign(512, 0);
    m_aPreviousCellIndex.assign(512, 0);
    lineRule=nullptr;
}

/**
 * Deconstructor.
 */
BreakIterator_th::~BreakIterator_th()
{
}

sal_Int32 SAL_CALL BreakIterator_th::previousCharacters( const OUString& Text,
    sal_Int32 nStartPos, const lang::Locale& rLocale,
    sal_Int16 nCharacterIteratorMode, sal_Int32 nCount, sal_Int32& nDone )
{
    if (nCharacterIteratorMode == CharacterIteratorMode::SKIPCELL ) {
        nDone = 0;
        if (nStartPos > 0) {    // for others to skip cell.
            makeIndex(Text, nStartPos);

            if (m_aNextCellIndex[nStartPos-1] == 0) // not a CTL character
                return BreakIterator_Unicode::previousCharacters(Text, nStartPos, rLocale,
                    nCharacterIteratorMode, nCount, nDone);
            else
            {
                while (nCount > 0 && m_aNextCellIndex[nStartPos - 1] > 0)
                {
                    nCount--; nDone++;
                    nStartPos = m_aPreviousCellIndex[nStartPos - 1];
                }
            }
        } else
            nStartPos = 0;
    } else { // for BS to delete one char.
        for (nDone = 0; nDone < nCount && nStartPos > 0; nDone++)
            Text.iterateCodePoints(&nStartPos, -1);
    }

    return nStartPos;
}

sal_Int32 SAL_CALL BreakIterator_th::nextCharacters(const OUString& Text,
    sal_Int32 nStartPos, const lang::Locale& rLocale,
    sal_Int16 nCharacterIteratorMode, sal_Int32 nCount, sal_Int32& nDone)
{
    sal_Int32 len = Text.getLength();
    if (nCharacterIteratorMode == CharacterIteratorMode::SKIPCELL ) {
        nDone = 0;
        if (nStartPos < len) {
            makeIndex(Text, nStartPos);

            if (m_aNextCellIndex[nStartPos] == 0) // not a CTL character
                return BreakIterator_Unicode::nextCharacters(Text, nStartPos, rLocale,
                    nCharacterIteratorMode, nCount, nDone);
            else
            {
                while (nCount > 0 && m_aNextCellIndex[nStartPos] > 0)
                {
                    nCount--; nDone++;
                    nStartPos = m_aNextCellIndex[nStartPos];
                }
            }
        } else
            nStartPos = len;
    } else {
        for (nDone = 0; nDone < nCount && nStartPos < Text.getLength(); nDone++)
            Text.iterateCodePoints(&nStartPos);
    }

    return nStartPos;
}

// Make sure line is broken on cell boundary if we implement cell iterator.
LineBreakResults SAL_CALL BreakIterator_th::getLineBreak(
    const OUString& Text, sal_Int32 nStartPos,
    const lang::Locale& rLocale, sal_Int32 nMinBreakPos,
    const LineBreakHyphenationOptions& hOptions,
    const LineBreakUserOptions& bOptions )
{
    LineBreakResults lbr = BreakIterator_Unicode::getLineBreak(Text, nStartPos,
                    rLocale, nMinBreakPos, hOptions, bOptions );
    if (lbr.breakIndex < Text.getLength()) {
        makeIndex(Text, lbr.breakIndex);
        lbr.breakIndex = m_aPreviousCellIndex[ lbr.breakIndex ];
    }
    return lbr;
}

#define SARA_AM 0x0E33

/*
 * cell composition states
 */

#define ST_COM  1   // Compose the following character with leading char and display in the same cell
#define ST_NXT  2   // display the following character in the next cell
#define ST_NDP  3   // non-display

const sal_Int16 thaiCompRel[MAX_CT][MAX_CT] = {
    //  C  N  C  L  F  F  F  B  B  B  T  A  A  A  A  A  A
    //  T  O  O  V  V  V  V  V  V  D  O  D  D  D  V  V  V
    //  R  N  N     1  2  3  1  2     N  1  2  3  1  2  3
    //  L     S                       E
    //  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // CTRL 0
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // NON  1
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_COM, ST_COM, ST_COM, ST_COM, ST_COM, ST_COM, ST_COM, ST_COM, ST_COM, ST_COM   }, // CONS 2
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // LV   3
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // FV1  4
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // FV2  5
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // FV3  6
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_COM, ST_COM, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // BV1  7
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_COM, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // BV2  8
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // BD   9
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // TONE 10
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // AD1  11
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // AD2  12
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // AD3  13
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_COM, ST_COM, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // AV1  14
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_COM, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT   }, // AV2  15
    {   ST_NDP, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_NXT, ST_COM, ST_NXT, ST_COM, ST_NXT, ST_NXT, ST_NXT, ST_NXT   } // AV3  16

};

const sal_uInt32 is_ST_COM = (1<<CT_CTRL)|(1<<CT_NON)|(1<<CT_CONS)|(1<<CT_TONE);

static sal_uInt16 getCombState(const sal_Unicode *text, sal_Int32 pos)
{
    sal_uInt16 ch1 = getCharType(text[pos]);
    sal_uInt16 ch2 = getCharType(text[pos+1]);

    if (text[pos+1] == SARA_AM) {
        if ((1 << ch1) & is_ST_COM)
            return  ST_COM;
        else
            ch2 = CT_AD1;
    }

    return thaiCompRel[ch1][ch2];
}


static sal_Int32 getACell(const sal_Unicode *text, sal_Int32 pos, sal_Int32 len)
{
    sal_uInt32 curr = 1;
    for (; pos + 1 < len && getCombState(text, pos) == ST_COM; curr++, pos++) {}
    return curr;
}

#define is_Thai(c)  (0x0e00 <= c && c <= 0x0e7f) // Unicode definition for Thai

void BreakIterator_th::makeIndex(const OUString& Text, sal_Int32 const nStartPos)
{
    if (Text != cachedText) {
        cachedText = Text;
        if (m_aNextCellIndex.size() < o3tl::make_unsigned(cachedText.getLength())) {
            m_aNextCellIndex.resize(cachedText.getLength());
            m_aPreviousCellIndex.resize(cachedText.getLength());
        }
        // reset nextCell for new Text
        m_aNextCellIndex.assign(cachedText.getLength(), 0);
    }
    else if (nStartPos >= Text.getLength() || m_aNextCellIndex[nStartPos] > 0
             || !is_Thai(Text[nStartPos]))
        return;

    const sal_Unicode* str = cachedText.getStr();
    sal_Int32 const len = cachedText.getLength();

    sal_Int32 startPos = nStartPos;
    while (startPos > 0 && is_Thai(str[startPos-1])) startPos--;
    sal_Int32 endPos = nStartPos;
    while (endPos < len && is_Thai(str[endPos])) endPos++;

    sal_Int32 start, end, pos;
    pos = start = end = startPos;

    assert(endPos >= 0 && o3tl::make_unsigned(endPos) <= m_aNextCellIndex.size());
    while (pos < endPos) {
        end += getACell(str, start, endPos);
        assert(end >= 0 && o3tl::make_unsigned(end) <= m_aNextCellIndex.size());
        while (pos < end) {
            m_aNextCellIndex[pos] = end;
            m_aPreviousCellIndex[pos] = start;
            pos++;
        }
        start = end;
    }
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

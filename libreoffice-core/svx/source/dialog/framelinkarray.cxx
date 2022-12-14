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

#include <svx/framelinkarray.hxx>

#include <math.h>
#include <vector>
#include <set>
#include <algorithm>
#include <tools/debug.hxx>
#include <tools/gen.hxx>
#include <vcl/canvastools.hxx>
#include <svx/sdr/primitive2d/sdrframeborderprimitive2d.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <basegfx/polygon/b2dpolygonclipper.hxx>

//#define OPTICAL_CHECK_CLIPRANGE_FOR_MERGED_CELL
#ifdef OPTICAL_CHECK_CLIPRANGE_FOR_MERGED_CELL
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <drawinglayer/primitive2d/PolygonHairlinePrimitive2D.hxx>
#endif

namespace svx::frame {

namespace {

class Cell
{
private:
    Style               maLeft;
    Style               maRight;
    Style               maTop;
    Style               maBottom;
    Style               maTLBR;
    Style               maBLTR;

    basegfx::B2DHomMatrix HelperCreateB2DHomMatrixFromB2DRange(
        const basegfx::B2DRange& rRange ) const;

public:
    tools::Long                mnAddLeft;
    tools::Long                mnAddRight;
    tools::Long                mnAddTop;
    tools::Long                mnAddBottom;

    SvxRotateMode       meRotMode;
    double              mfOrientation;

    bool                mbMergeOrig;
    bool                mbOverlapX;
    bool                mbOverlapY;

public:
    explicit            Cell();

    void SetStyleLeft(const Style& rStyle) { maLeft = rStyle; }
    void SetStyleRight(const Style& rStyle) { maRight = rStyle; }
    void SetStyleTop(const Style& rStyle) { maTop = rStyle; }
    void SetStyleBottom(const Style& rStyle) { maBottom = rStyle; }
    void SetStyleTLBR(const Style& rStyle) { maTLBR = rStyle; }
    void SetStyleBLTR(const Style& rStyle) { maBLTR = rStyle; }

    const Style& GetStyleLeft() const { return maLeft; }
    const Style& GetStyleRight() const { return maRight; }
    const Style& GetStyleTop() const { return maTop; }
    const Style& GetStyleBottom() const { return maBottom; }
    const Style& GetStyleTLBR() const { return maTLBR; }
    const Style& GetStyleBLTR() const { return maBLTR; }

    bool                IsMerged() const { return mbMergeOrig || mbOverlapX || mbOverlapY; }
    bool                IsRotated() const { return mfOrientation != 0.0; }

    void                MirrorSelfX();

    basegfx::B2DHomMatrix CreateCoordinateSystemSingleCell(
        const Array& rArray, size_t nCol, size_t nRow ) const;
    basegfx::B2DHomMatrix CreateCoordinateSystemMergedCell(
        const Array& rArray, size_t nColLeft, size_t nRowTop, size_t nColRight, size_t nRowBottom ) const;
};

}

typedef std::vector< Cell >     CellVec;

basegfx::B2DHomMatrix Cell::HelperCreateB2DHomMatrixFromB2DRange(
    const basegfx::B2DRange& rRange ) const
{
    if( rRange.isEmpty() )
        return basegfx::B2DHomMatrix();

    basegfx::B2DPoint aOrigin(rRange.getMinimum());
    basegfx::B2DVector aX(rRange.getWidth(), 0.0);
    basegfx::B2DVector aY(0.0, rRange.getHeight());

    if (IsRotated() && SvxRotateMode::SVX_ROTATE_MODE_STANDARD != meRotMode )
    {
        // when rotated, adapt values. Get Skew (cos/sin == 1/tan)
        const double fSkew(aY.getY() * (cos(mfOrientation) / sin(mfOrientation)));

        switch (meRotMode)
        {
        case SvxRotateMode::SVX_ROTATE_MODE_TOP:
            // shear Y-Axis
            aY.setX(-fSkew);
            break;
        case SvxRotateMode::SVX_ROTATE_MODE_CENTER:
            // shear origin half, Y full
            aOrigin.setX(aOrigin.getX() + (fSkew * 0.5));
            aY.setX(-fSkew);
            break;
        case SvxRotateMode::SVX_ROTATE_MODE_BOTTOM:
            // shear origin full, Y full
            aOrigin.setX(aOrigin.getX() + fSkew);
            aY.setX(-fSkew);
            break;
        default: // SvxRotateMode::SVX_ROTATE_MODE_STANDARD, already excluded above
            break;
        }
    }

    // use column vectors as coordinate axes, homogen column for translation
    return basegfx::utils::createCoordinateSystemTransform( aOrigin, aX, aY );
}

basegfx::B2DHomMatrix Cell::CreateCoordinateSystemSingleCell(
    const Array& rArray, size_t nCol, size_t nRow) const
{
    const Point aPoint( rArray.GetColPosition( nCol ), rArray.GetRowPosition( nRow ) );
    const Size aSize( rArray.GetColWidth( nCol, nCol ) + 1, rArray.GetRowHeight( nRow, nRow ) + 1 );
    const basegfx::B2DRange aRange( vcl::unotools::b2DRectangleFromRectangle( tools::Rectangle( aPoint, aSize ) ) );

    return HelperCreateB2DHomMatrixFromB2DRange( aRange );
}

basegfx::B2DHomMatrix Cell::CreateCoordinateSystemMergedCell(
    const Array& rArray, size_t nColLeft, size_t nRowTop, size_t nColRight, size_t nRowBottom) const
{
    basegfx::B2DRange aRange( rArray.GetB2DRange(
        nColLeft, nRowTop, nColRight, nRowBottom ) );

    // adjust rectangle for partly visible merged cells
    if( IsMerged() )
    {
        // not *sure* what exactly this is good for,
        // it is just a hard set extension at merged cells,
        // probably *should* be included in the above extended
        // GetColPosition/GetColWidth already. This might be
        // added due to GetColPosition/GetColWidth not working
        // correctly over PageChanges (if used), but not sure.
        aRange.expand(
            basegfx::B2DRange(
                aRange.getMinX() - mnAddLeft,
                aRange.getMinY() - mnAddTop,
                aRange.getMaxX() + mnAddRight,
                aRange.getMaxY() + mnAddBottom ) );
    }

    return HelperCreateB2DHomMatrixFromB2DRange( aRange );
}

Cell::Cell() :
    mnAddLeft( 0 ),
    mnAddRight( 0 ),
    mnAddTop( 0 ),
    mnAddBottom( 0 ),
    meRotMode(SvxRotateMode::SVX_ROTATE_MODE_STANDARD ),
    mfOrientation( 0.0 ),
    mbMergeOrig( false ),
    mbOverlapX( false ),
    mbOverlapY( false )
{
}

void Cell::MirrorSelfX()
{
    std::swap( maLeft, maRight );
    std::swap( mnAddLeft, mnAddRight );
    maLeft.MirrorSelf();
    maRight.MirrorSelf();
    mfOrientation = -mfOrientation;
}


static void lclRecalcCoordVec( std::vector<tools::Long>& rCoords, const std::vector<tools::Long>& rSizes )
{
    DBG_ASSERT( rCoords.size() == rSizes.size() + 1, "lclRecalcCoordVec - inconsistent vectors" );
    auto aCIt = rCoords.begin();
    for( const auto& rSize : rSizes )
    {
        *(aCIt + 1) = *aCIt + rSize;
        ++aCIt;
    }
}

static void lclSetMergedRange( CellVec& rCells, size_t nWidth, size_t nFirstCol, size_t nFirstRow, size_t nLastCol, size_t nLastRow )
{
    for( size_t nCol = nFirstCol; nCol <= nLastCol; ++nCol )
    {
        for( size_t nRow = nFirstRow; nRow <= nLastRow; ++nRow )
        {
            Cell& rCell = rCells[ nRow * nWidth + nCol ];
            rCell.mbMergeOrig = false;
            rCell.mbOverlapX = nCol > nFirstCol;
            rCell.mbOverlapY = nRow > nFirstRow;
        }
    }
    rCells[ nFirstRow * nWidth + nFirstCol ].mbMergeOrig = true;
}


const Style OBJ_STYLE_NONE;
const Cell OBJ_CELL_NONE;

struct ArrayImpl
{
    CellVec             maCells;
    std::vector<tools::Long>   maWidths;
    std::vector<tools::Long>   maHeights;
    mutable std::vector<tools::Long>     maXCoords;
    mutable std::vector<tools::Long>     maYCoords;
    size_t              mnWidth;
    size_t              mnHeight;
    size_t              mnFirstClipCol;
    size_t              mnFirstClipRow;
    size_t              mnLastClipCol;
    size_t              mnLastClipRow;
    mutable bool        mbXCoordsDirty;
    mutable bool        mbYCoordsDirty;
    bool                mbMayHaveCellRotation;

    explicit            ArrayImpl( size_t nWidth, size_t nHeight );

    bool         IsValidPos( size_t nCol, size_t nRow ) const
                            { return (nCol < mnWidth) && (nRow < mnHeight); }
    size_t       GetIndex( size_t nCol, size_t nRow ) const
                            { return nRow * mnWidth + nCol; }

    const Cell&         GetCell( size_t nCol, size_t nRow ) const;
    Cell&               GetCellAcc( size_t nCol, size_t nRow );

    size_t              GetMergedFirstCol( size_t nCol, size_t nRow ) const;
    size_t              GetMergedFirstRow( size_t nCol, size_t nRow ) const;
    size_t              GetMergedLastCol( size_t nCol, size_t nRow ) const;
    size_t              GetMergedLastRow( size_t nCol, size_t nRow ) const;

    const Cell&         GetMergedOriginCell( size_t nCol, size_t nRow ) const;
    const Cell&         GetMergedLastCell( size_t nCol, size_t nRow ) const;

    bool                IsMergedOverlappedLeft( size_t nCol, size_t nRow ) const;
    bool                IsMergedOverlappedRight( size_t nCol, size_t nRow ) const;
    bool                IsMergedOverlappedTop( size_t nCol, size_t nRow ) const;
    bool                IsMergedOverlappedBottom( size_t nCol, size_t nRow ) const;

    bool                IsInClipRange( size_t nCol, size_t nRow ) const;
    bool                IsColInClipRange( size_t nCol ) const;
    bool                IsRowInClipRange( size_t nRow ) const;

    bool                OverlapsClipRange( size_t nFirstCol, size_t nFirstRow, size_t nLastCol, size_t nLastRow ) const;

    size_t       GetMirrorCol( size_t nCol ) const { return mnWidth - nCol - 1; }

    tools::Long                GetColPosition( size_t nCol ) const;
    tools::Long                GetRowPosition( size_t nRow ) const;

    bool                HasCellRotation() const;
};

ArrayImpl::ArrayImpl( size_t nWidth, size_t nHeight ) :
    mnWidth( nWidth ),
    mnHeight( nHeight ),
    mnFirstClipCol( 0 ),
    mnFirstClipRow( 0 ),
    mnLastClipCol( nWidth - 1 ),
    mnLastClipRow( nHeight - 1 ),
    mbXCoordsDirty( false ),
    mbYCoordsDirty( false ),
    mbMayHaveCellRotation( false )
{
    // default-construct all vectors
    maCells.resize( mnWidth * mnHeight );
    maWidths.resize( mnWidth, 0 );
    maHeights.resize( mnHeight, 0 );
    maXCoords.resize( mnWidth + 1, 0 );
    maYCoords.resize( mnHeight + 1, 0 );
}

const Cell& ArrayImpl::GetCell( size_t nCol, size_t nRow ) const
{
    return IsValidPos( nCol, nRow ) ? maCells[ GetIndex( nCol, nRow ) ] : OBJ_CELL_NONE;
}

Cell& ArrayImpl::GetCellAcc( size_t nCol, size_t nRow )
{
    static Cell aDummy;
    return IsValidPos( nCol, nRow ) ? maCells[ GetIndex( nCol, nRow ) ] : aDummy;
}

size_t ArrayImpl::GetMergedFirstCol( size_t nCol, size_t nRow ) const
{
    size_t nFirstCol = nCol;
    while( (nFirstCol > 0) && GetCell( nFirstCol, nRow ).mbOverlapX ) --nFirstCol;
    return nFirstCol;
}

size_t ArrayImpl::GetMergedFirstRow( size_t nCol, size_t nRow ) const
{
    size_t nFirstRow = nRow;
    while( (nFirstRow > 0) && GetCell( nCol, nFirstRow ).mbOverlapY ) --nFirstRow;
    return nFirstRow;
}

size_t ArrayImpl::GetMergedLastCol( size_t nCol, size_t nRow ) const
{
    size_t nLastCol = nCol + 1;
    while( (nLastCol < mnWidth) && GetCell( nLastCol, nRow ).mbOverlapX ) ++nLastCol;
    return nLastCol - 1;
}

size_t ArrayImpl::GetMergedLastRow( size_t nCol, size_t nRow ) const
{
    size_t nLastRow = nRow + 1;
    while( (nLastRow < mnHeight) && GetCell( nCol, nLastRow ).mbOverlapY ) ++nLastRow;
    return nLastRow - 1;
}

const Cell& ArrayImpl::GetMergedOriginCell( size_t nCol, size_t nRow ) const
{
    return GetCell( GetMergedFirstCol( nCol, nRow ), GetMergedFirstRow( nCol, nRow ) );
}

const Cell& ArrayImpl::GetMergedLastCell( size_t nCol, size_t nRow ) const
{
    return GetCell( GetMergedLastCol( nCol, nRow ), GetMergedLastRow( nCol, nRow ) );
}

bool ArrayImpl::IsMergedOverlappedLeft( size_t nCol, size_t nRow ) const
{
    const Cell& rCell = GetCell( nCol, nRow );
    return rCell.mbOverlapX || (rCell.mnAddLeft > 0);
}

bool ArrayImpl::IsMergedOverlappedRight( size_t nCol, size_t nRow ) const
{
    return GetCell( nCol + 1, nRow ).mbOverlapX || (GetCell( nCol, nRow ).mnAddRight > 0);
}

bool ArrayImpl::IsMergedOverlappedTop( size_t nCol, size_t nRow ) const
{
    const Cell& rCell = GetCell( nCol, nRow );
    return rCell.mbOverlapY || (rCell.mnAddTop > 0);
}

bool ArrayImpl::IsMergedOverlappedBottom( size_t nCol, size_t nRow ) const
{
    return GetCell( nCol, nRow + 1 ).mbOverlapY || (GetCell( nCol, nRow ).mnAddBottom > 0);
}

bool ArrayImpl::IsColInClipRange( size_t nCol ) const
{
    return (mnFirstClipCol <= nCol) && (nCol <= mnLastClipCol);
}

bool ArrayImpl::IsRowInClipRange( size_t nRow ) const
{
    return (mnFirstClipRow <= nRow) && (nRow <= mnLastClipRow);
}

bool ArrayImpl::OverlapsClipRange( size_t nFirstCol, size_t nFirstRow, size_t nLastCol, size_t nLastRow ) const
{
    if(nLastCol < mnFirstClipCol)
        return false;

    if(nFirstCol > mnLastClipCol)
        return false;

    if(nLastRow < mnFirstClipRow)
        return false;

    if(nFirstRow > mnLastClipRow)
        return false;

    return true;
}

bool ArrayImpl::IsInClipRange( size_t nCol, size_t nRow ) const
{
    return IsColInClipRange( nCol ) && IsRowInClipRange( nRow );
}

tools::Long ArrayImpl::GetColPosition( size_t nCol ) const
{
    if( mbXCoordsDirty )
    {
        lclRecalcCoordVec( maXCoords, maWidths );
        mbXCoordsDirty = false;
    }
    return maXCoords[ nCol ];
}

tools::Long ArrayImpl::GetRowPosition( size_t nRow ) const
{
    if( mbYCoordsDirty )
    {
        lclRecalcCoordVec( maYCoords, maHeights );
        mbYCoordsDirty = false;
    }
    return maYCoords[ nRow ];
}

bool ArrayImpl::HasCellRotation() const
{
    // check cell array
    for (const auto& aCell : maCells)
    {
        if (aCell.IsRotated())
        {
            return true;
        }
    }

    return false;
}

namespace {

class MergedCellIterator
{
public:
    explicit            MergedCellIterator( const Array& rArray, size_t nCol, size_t nRow );

    bool         Is() const { return (mnCol <= mnLastCol) && (mnRow <= mnLastRow); }
    size_t       Col() const { return mnCol; }
    size_t       Row() const { return mnRow; }

    MergedCellIterator& operator++();

private:
    size_t              mnFirstCol;
    size_t              mnFirstRow;
    size_t              mnLastCol;
    size_t              mnLastRow;
    size_t              mnCol;
    size_t              mnRow;
};

}

MergedCellIterator::MergedCellIterator( const Array& rArray, size_t nCol, size_t nRow )
{
    DBG_ASSERT( rArray.IsMerged( nCol, nRow ), "svx::frame::MergedCellIterator::MergedCellIterator - not in merged range" );
    rArray.GetMergedRange( mnFirstCol, mnFirstRow, mnLastCol, mnLastRow, nCol, nRow );
    mnCol = mnFirstCol;
    mnRow = mnFirstRow;
}

MergedCellIterator& MergedCellIterator::operator++()
{
    DBG_ASSERT( Is(), "svx::frame::MergedCellIterator::operator++() - already invalid" );
    if( ++mnCol > mnLastCol )
    {
        mnCol = mnFirstCol;
        ++mnRow;
    }
    return *this;
}


#define DBG_FRAME_CHECK( cond, funcname, error )        DBG_ASSERT( cond, "svx::frame::Array::" funcname " - " error )
#define DBG_FRAME_CHECK_COL( col, funcname )            DBG_FRAME_CHECK( (col) < GetColCount(), funcname, "invalid column index" )
#define DBG_FRAME_CHECK_ROW( row, funcname )            DBG_FRAME_CHECK( (row) < GetRowCount(), funcname, "invalid row index" )
#define DBG_FRAME_CHECK_COLROW( col, row, funcname )    DBG_FRAME_CHECK( ((col) < GetColCount()) && ((row) < GetRowCount()), funcname, "invalid cell index" )
#define DBG_FRAME_CHECK_COL_1( col, funcname )          DBG_FRAME_CHECK( (col) <= GetColCount(), funcname, "invalid column index" )
#define DBG_FRAME_CHECK_ROW_1( row, funcname )          DBG_FRAME_CHECK( (row) <= GetRowCount(), funcname, "invalid row index" )


#define CELL( col, row )        mxImpl->GetCell( col, row )
#define CELLACC( col, row )     mxImpl->GetCellAcc( col, row )
#define ORIGCELL( col, row )    mxImpl->GetMergedOriginCell( col, row )
#define LASTCELL( col, row )    mxImpl->GetMergedLastCell( col, row )


Array::Array()
{
    Initialize( 0, 0 );
}

Array::~Array()
{
}

// array size and column/row indexes
void Array::Initialize( size_t nWidth, size_t nHeight )
{
    mxImpl.reset( new ArrayImpl( nWidth, nHeight ) );
}

size_t Array::GetColCount() const
{
    return mxImpl->mnWidth;
}

size_t Array::GetRowCount() const
{
    return mxImpl->mnHeight;
}

size_t Array::GetCellCount() const
{
    return mxImpl->maCells.size();
}

size_t Array::GetCellIndex( size_t nCol, size_t nRow, bool bRTL ) const
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "GetCellIndex" );
    if (bRTL)
        nCol = mxImpl->GetMirrorCol(nCol);
    return mxImpl->GetIndex( nCol, nRow );
}

// cell border styles
void Array::SetCellStyleLeft( size_t nCol, size_t nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleLeft" );
    CELLACC( nCol, nRow ).SetStyleLeft(rStyle);
}

void Array::SetCellStyleRight( size_t nCol, size_t nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleRight" );
    CELLACC( nCol, nRow ).SetStyleRight(rStyle);
}

void Array::SetCellStyleTop( size_t nCol, size_t nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleTop" );
    CELLACC( nCol, nRow ).SetStyleTop(rStyle);
}

void Array::SetCellStyleBottom( size_t nCol, size_t nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleBottom" );
    CELLACC( nCol, nRow ).SetStyleBottom(rStyle);
}

void Array::SetCellStyleTLBR( size_t nCol, size_t nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleTLBR" );
    CELLACC( nCol, nRow ).SetStyleTLBR(rStyle);
}

void Array::SetCellStyleBLTR( size_t nCol, size_t nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleBLTR" );
    CELLACC( nCol, nRow ).SetStyleBLTR(rStyle);
}

void Array::SetCellStyleDiag( size_t nCol, size_t nRow, const Style& rTLBR, const Style& rBLTR )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetCellStyleDiag" );
    Cell& rCell = CELLACC( nCol, nRow );
    rCell.SetStyleTLBR(rTLBR);
    rCell.SetStyleBLTR(rBLTR);
}

void Array::SetColumnStyleLeft( size_t nCol, const Style& rStyle )
{
    DBG_FRAME_CHECK_COL( nCol, "SetColumnStyleLeft" );
    for( size_t nRow = 0; nRow < mxImpl->mnHeight; ++nRow )
        SetCellStyleLeft( nCol, nRow, rStyle );
}

void Array::SetColumnStyleRight( size_t nCol, const Style& rStyle )
{
    DBG_FRAME_CHECK_COL( nCol, "SetColumnStyleRight" );
    for( size_t nRow = 0; nRow < mxImpl->mnHeight; ++nRow )
        SetCellStyleRight( nCol, nRow, rStyle );
}

void Array::SetRowStyleTop( size_t nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_ROW( nRow, "SetRowStyleTop" );
    for( size_t nCol = 0; nCol < mxImpl->mnWidth; ++nCol )
        SetCellStyleTop( nCol, nRow, rStyle );
}

void Array::SetRowStyleBottom( size_t nRow, const Style& rStyle )
{
    DBG_FRAME_CHECK_ROW( nRow, "SetRowStyleBottom" );
    for( size_t nCol = 0; nCol < mxImpl->mnWidth; ++nCol )
        SetCellStyleBottom( nCol, nRow, rStyle );
}

void Array::SetCellRotation(size_t nCol, size_t nRow, SvxRotateMode eRotMode, double fOrientation)
{
    DBG_FRAME_CHECK_COLROW(nCol, nRow, "SetCellRotation");
    Cell& rTarget = CELLACC(nCol, nRow);
    rTarget.meRotMode = eRotMode;
    rTarget.mfOrientation = fOrientation;

    if (!mxImpl->mbMayHaveCellRotation)
    {
        // activate once when a cell gets actually rotated to allow fast
        // answering HasCellRotation() calls
        mxImpl->mbMayHaveCellRotation = rTarget.IsRotated();
    }
}

bool Array::HasCellRotation() const
{
    if (!mxImpl->mbMayHaveCellRotation)
    {
        // never set, no need to check
        return false;
    }

    return mxImpl->HasCellRotation();
}

const Style& Array::GetCellStyleLeft( size_t nCol, size_t nRow ) const
{
    // outside clipping rows or overlapped in merged cells: invisible
    if( !mxImpl->IsRowInClipRange( nRow ) || mxImpl->IsMergedOverlappedLeft( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // left clipping border: always own left style
    if( nCol == mxImpl->mnFirstClipCol )
        return ORIGCELL( nCol, nRow ).GetStyleLeft();
    // right clipping border: always right style of left neighbor cell
    if( nCol == mxImpl->mnLastClipCol + 1 )
        return ORIGCELL( nCol - 1, nRow ).GetStyleRight();
    // outside clipping columns: invisible
    if( !mxImpl->IsColInClipRange( nCol ) )
        return OBJ_STYLE_NONE;
    // inside clipping range: maximum of own left style and right style of left neighbor cell
    return std::max( ORIGCELL( nCol, nRow ).GetStyleLeft(), ORIGCELL( nCol - 1, nRow ).GetStyleRight() );
}

const Style& Array::GetCellStyleRight( size_t nCol, size_t nRow ) const
{
    // outside clipping rows or overlapped in merged cells: invisible
    if( !mxImpl->IsRowInClipRange( nRow ) || mxImpl->IsMergedOverlappedRight( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // left clipping border: always left style of right neighbor cell
    if( nCol + 1 == mxImpl->mnFirstClipCol )
        return ORIGCELL( nCol + 1, nRow ).GetStyleLeft();
    // right clipping border: always own right style
    if( nCol == mxImpl->mnLastClipCol )
        return LASTCELL( nCol, nRow ).GetStyleRight();
    // outside clipping columns: invisible
    if( !mxImpl->IsColInClipRange( nCol ) )
        return OBJ_STYLE_NONE;
    // inside clipping range: maximum of own right style and left style of right neighbor cell
    return std::max( ORIGCELL( nCol, nRow ).GetStyleRight(), ORIGCELL( nCol + 1, nRow ).GetStyleLeft() );
}

const Style& Array::GetCellStyleTop( size_t nCol, size_t nRow ) const
{
    // outside clipping columns or overlapped in merged cells: invisible
    if( !mxImpl->IsColInClipRange( nCol ) || mxImpl->IsMergedOverlappedTop( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // top clipping border: always own top style
    if( nRow == mxImpl->mnFirstClipRow )
        return ORIGCELL( nCol, nRow ).GetStyleTop();
    // bottom clipping border: always bottom style of top neighbor cell
    if( nRow == mxImpl->mnLastClipRow + 1 )
        return ORIGCELL( nCol, nRow - 1 ).GetStyleBottom();
    // outside clipping rows: invisible
    if( !mxImpl->IsRowInClipRange( nRow ) )
        return OBJ_STYLE_NONE;
    // inside clipping range: maximum of own top style and bottom style of top neighbor cell
    return std::max( ORIGCELL( nCol, nRow ).GetStyleTop(), ORIGCELL( nCol, nRow - 1 ).GetStyleBottom() );
}

const Style& Array::GetCellStyleBottom( size_t nCol, size_t nRow ) const
{
    // outside clipping columns or overlapped in merged cells: invisible
    if( !mxImpl->IsColInClipRange( nCol ) || mxImpl->IsMergedOverlappedBottom( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // top clipping border: always top style of bottom neighbor cell
    if( nRow + 1 == mxImpl->mnFirstClipRow )
        return ORIGCELL( nCol, nRow + 1 ).GetStyleTop();
    // bottom clipping border: always own bottom style
    if( nRow == mxImpl->mnLastClipRow )
        return LASTCELL( nCol, nRow ).GetStyleBottom();
    // outside clipping rows: invisible
    if( !mxImpl->IsRowInClipRange( nRow ) )
        return OBJ_STYLE_NONE;
    // inside clipping range: maximum of own bottom style and top style of bottom neighbor cell
    return std::max( ORIGCELL( nCol, nRow ).GetStyleBottom(), ORIGCELL( nCol, nRow + 1 ).GetStyleTop() );
}

const Style& Array::GetCellStyleTLBR( size_t nCol, size_t nRow ) const
{
    return CELL( nCol, nRow ).GetStyleTLBR();
}

const Style& Array::GetCellStyleBLTR( size_t nCol, size_t nRow ) const
{
    return CELL( nCol, nRow ).GetStyleBLTR();
}

const Style& Array::GetCellStyleTL( size_t nCol, size_t nRow ) const
{
    // not in clipping range: always invisible
    if( !mxImpl->IsInClipRange( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // return style only for top-left cell
    size_t nFirstCol = mxImpl->GetMergedFirstCol( nCol, nRow );
    size_t nFirstRow = mxImpl->GetMergedFirstRow( nCol, nRow );
    return ((nCol == nFirstCol) && (nRow == nFirstRow)) ?
        CELL( nFirstCol, nFirstRow ).GetStyleTLBR() : OBJ_STYLE_NONE;
}

const Style& Array::GetCellStyleBR( size_t nCol, size_t nRow ) const
{
    // not in clipping range: always invisible
    if( !mxImpl->IsInClipRange( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // return style only for bottom-right cell
    size_t nLastCol = mxImpl->GetMergedLastCol( nCol, nRow );
    size_t nLastRow = mxImpl->GetMergedLastRow( nCol, nRow );
    return ((nCol == nLastCol) && (nRow == nLastRow)) ?
        CELL( mxImpl->GetMergedFirstCol( nCol, nRow ), mxImpl->GetMergedFirstRow( nCol, nRow ) ).GetStyleTLBR() : OBJ_STYLE_NONE;
}

const Style& Array::GetCellStyleBL( size_t nCol, size_t nRow ) const
{
    // not in clipping range: always invisible
    if( !mxImpl->IsInClipRange( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // return style only for bottom-left cell
    size_t nFirstCol = mxImpl->GetMergedFirstCol( nCol, nRow );
    size_t nLastRow = mxImpl->GetMergedLastRow( nCol, nRow );
    return ((nCol == nFirstCol) && (nRow == nLastRow)) ?
        CELL( nFirstCol, mxImpl->GetMergedFirstRow( nCol, nRow ) ).GetStyleBLTR() : OBJ_STYLE_NONE;
}

const Style& Array::GetCellStyleTR( size_t nCol, size_t nRow ) const
{
    // not in clipping range: always invisible
    if( !mxImpl->IsInClipRange( nCol, nRow ) )
        return OBJ_STYLE_NONE;
    // return style only for top-right cell
    size_t nFirstRow = mxImpl->GetMergedFirstRow( nCol, nRow );
    size_t nLastCol = mxImpl->GetMergedLastCol( nCol, nRow );
    return ((nCol == nLastCol) && (nRow == nFirstRow)) ?
        CELL( mxImpl->GetMergedFirstCol( nCol, nRow ), nFirstRow ).GetStyleBLTR() : OBJ_STYLE_NONE;
}

// cell merging
void Array::SetMergedRange( size_t nFirstCol, size_t nFirstRow, size_t nLastCol, size_t nLastRow )
{
    DBG_FRAME_CHECK_COLROW( nFirstCol, nFirstRow, "SetMergedRange" );
    DBG_FRAME_CHECK_COLROW( nLastCol, nLastRow, "SetMergedRange" );
#if OSL_DEBUG_LEVEL >= 2
    {
        bool bFound = false;
        for( size_t nCurrCol = nFirstCol; !bFound && (nCurrCol <= nLastCol); ++nCurrCol )
            for( size_t nCurrRow = nFirstRow; !bFound && (nCurrRow <= nLastRow); ++nCurrRow )
                bFound = CELL( nCurrCol, nCurrRow ).IsMerged();
        DBG_FRAME_CHECK( !bFound, "SetMergedRange", "overlapping merged ranges" );
    }
#endif
    if( mxImpl->IsValidPos( nFirstCol, nFirstRow ) && mxImpl->IsValidPos( nLastCol, nLastRow ) )
        lclSetMergedRange( mxImpl->maCells, mxImpl->mnWidth, nFirstCol, nFirstRow, nLastCol, nLastRow );
}

void Array::SetAddMergedLeftSize( size_t nCol, size_t nRow, tools::Long nAddSize )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetAddMergedLeftSize" );
    DBG_FRAME_CHECK( mxImpl->GetMergedFirstCol( nCol, nRow ) == 0, "SetAddMergedLeftSize", "additional border inside array" );
    for( MergedCellIterator aIt( *this, nCol, nRow ); aIt.Is(); ++aIt )
        CELLACC( aIt.Col(), aIt.Row() ).mnAddLeft = nAddSize;
}

void Array::SetAddMergedRightSize( size_t nCol, size_t nRow, tools::Long nAddSize )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetAddMergedRightSize" );
    DBG_FRAME_CHECK( mxImpl->GetMergedLastCol( nCol, nRow ) + 1 == mxImpl->mnWidth, "SetAddMergedRightSize", "additional border inside array" );
    for( MergedCellIterator aIt( *this, nCol, nRow ); aIt.Is(); ++aIt )
        CELLACC( aIt.Col(), aIt.Row() ).mnAddRight = nAddSize;
}

void Array::SetAddMergedTopSize( size_t nCol, size_t nRow, tools::Long nAddSize )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetAddMergedTopSize" );
    DBG_FRAME_CHECK( mxImpl->GetMergedFirstRow( nCol, nRow ) == 0, "SetAddMergedTopSize", "additional border inside array" );
    for( MergedCellIterator aIt( *this, nCol, nRow ); aIt.Is(); ++aIt )
        CELLACC( aIt.Col(), aIt.Row() ).mnAddTop = nAddSize;
}

void Array::SetAddMergedBottomSize( size_t nCol, size_t nRow, tools::Long nAddSize )
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "SetAddMergedBottomSize" );
    DBG_FRAME_CHECK( mxImpl->GetMergedLastRow( nCol, nRow ) + 1 == mxImpl->mnHeight, "SetAddMergedBottomSize", "additional border inside array" );
    for( MergedCellIterator aIt( *this, nCol, nRow ); aIt.Is(); ++aIt )
        CELLACC( aIt.Col(), aIt.Row() ).mnAddBottom = nAddSize;
}

bool Array::IsMerged( size_t nCol, size_t nRow ) const
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "IsMerged" );
    return CELL( nCol, nRow ).IsMerged();
}

void Array::GetMergedOrigin( size_t& rnFirstCol, size_t& rnFirstRow, size_t nCol, size_t nRow ) const
{
    DBG_FRAME_CHECK_COLROW( nCol, nRow, "GetMergedOrigin" );
    rnFirstCol = mxImpl->GetMergedFirstCol( nCol, nRow );
    rnFirstRow = mxImpl->GetMergedFirstRow( nCol, nRow );
}

void Array::GetMergedRange( size_t& rnFirstCol, size_t& rnFirstRow,
        size_t& rnLastCol, size_t& rnLastRow, size_t nCol, size_t nRow ) const
{
    GetMergedOrigin( rnFirstCol, rnFirstRow, nCol, nRow );
    rnLastCol = mxImpl->GetMergedLastCol( nCol, nRow );
    rnLastRow = mxImpl->GetMergedLastRow( nCol, nRow );
}

// clipping
void Array::SetClipRange( size_t nFirstCol, size_t nFirstRow, size_t nLastCol, size_t nLastRow )
{
    DBG_FRAME_CHECK_COLROW( nFirstCol, nFirstRow, "SetClipRange" );
    DBG_FRAME_CHECK_COLROW( nLastCol, nLastRow, "SetClipRange" );
    mxImpl->mnFirstClipCol = nFirstCol;
    mxImpl->mnFirstClipRow = nFirstRow;
    mxImpl->mnLastClipCol = nLastCol;
    mxImpl->mnLastClipRow = nLastRow;
}

// cell coordinates
void Array::SetXOffset( tools::Long nXOffset )
{
    mxImpl->maXCoords[ 0 ] = nXOffset;
    mxImpl->mbXCoordsDirty = true;
}

void Array::SetYOffset( tools::Long nYOffset )
{
    mxImpl->maYCoords[ 0 ] = nYOffset;
    mxImpl->mbYCoordsDirty = true;
}

void Array::SetColWidth( size_t nCol, tools::Long nWidth )
{
    DBG_FRAME_CHECK_COL( nCol, "SetColWidth" );
    mxImpl->maWidths[ nCol ] = nWidth;
    mxImpl->mbXCoordsDirty = true;
}

void Array::SetRowHeight( size_t nRow, tools::Long nHeight )
{
    DBG_FRAME_CHECK_ROW( nRow, "SetRowHeight" );
    mxImpl->maHeights[ nRow ] = nHeight;
    mxImpl->mbYCoordsDirty = true;
}

void Array::SetAllColWidths( tools::Long nWidth )
{
    std::fill( mxImpl->maWidths.begin(), mxImpl->maWidths.end(), nWidth );
    mxImpl->mbXCoordsDirty = true;
}

void Array::SetAllRowHeights( tools::Long nHeight )
{
    std::fill( mxImpl->maHeights.begin(), mxImpl->maHeights.end(), nHeight );
    mxImpl->mbYCoordsDirty = true;
}

tools::Long Array::GetColPosition( size_t nCol ) const
{
    DBG_FRAME_CHECK_COL_1( nCol, "GetColPosition" );
    return mxImpl->GetColPosition( nCol );
}

tools::Long Array::GetRowPosition( size_t nRow ) const
{
    DBG_FRAME_CHECK_ROW_1( nRow, "GetRowPosition" );
    return mxImpl->GetRowPosition( nRow );
}

tools::Long Array::GetColWidth( size_t nFirstCol, size_t nLastCol ) const
{
    DBG_FRAME_CHECK_COL( nFirstCol, "GetColWidth" );
    DBG_FRAME_CHECK_COL( nLastCol, "GetColWidth" );
    return GetColPosition( nLastCol + 1 ) - GetColPosition( nFirstCol );
}

tools::Long Array::GetRowHeight( size_t nFirstRow, size_t nLastRow ) const
{
    DBG_FRAME_CHECK_ROW( nFirstRow, "GetRowHeight" );
    DBG_FRAME_CHECK_ROW( nLastRow, "GetRowHeight" );
    return GetRowPosition( nLastRow + 1 ) - GetRowPosition( nFirstRow );
}

tools::Long Array::GetWidth() const
{
    return GetColPosition( mxImpl->mnWidth ) - GetColPosition( 0 );
}

tools::Long Array::GetHeight() const
{
    return GetRowPosition( mxImpl->mnHeight ) - GetRowPosition( 0 );
}

basegfx::B2DRange Array::GetCellRange( size_t nCol, size_t nRow, bool bExpandMerged ) const
{
    if(bExpandMerged)
    {
        // get the Range of the fully expanded cell (if merged)
        const size_t nFirstCol(mxImpl->GetMergedFirstCol( nCol, nRow ));
        const size_t nFirstRow(mxImpl->GetMergedFirstRow( nCol, nRow ));
        const size_t nLastCol(mxImpl->GetMergedLastCol( nCol, nRow ));
        const size_t nLastRow(mxImpl->GetMergedLastRow( nCol, nRow ));
        const Point aPoint( GetColPosition( nFirstCol ), GetRowPosition( nFirstRow ) );
        const Size aSize( GetColWidth( nFirstCol, nLastCol ) + 1, GetRowHeight( nFirstRow, nLastRow ) + 1 );
        tools::Rectangle aRect(aPoint, aSize);

        // adjust rectangle for partly visible merged cells
        const Cell& rCell = CELL( nCol, nRow );

        if( rCell.IsMerged() )
        {
            // not *sure* what exactly this is good for,
            // it is just a hard set extension at merged cells,
            // probably *should* be included in the above extended
            // GetColPosition/GetColWidth already. This might be
            // added due to GetColPosition/GetColWidth not working
            // correctly over PageChanges (if used), but not sure.
            aRect.AdjustLeft( -(rCell.mnAddLeft) );
            aRect.AdjustRight(rCell.mnAddRight );
            aRect.AdjustTop( -(rCell.mnAddTop) );
            aRect.AdjustBottom(rCell.mnAddBottom );
        }

        return vcl::unotools::b2DRectangleFromRectangle(aRect);
    }
    else
    {
        const Point aPoint( GetColPosition( nCol ), GetRowPosition( nRow ) );
        const Size aSize( GetColWidth( nCol, nCol ) + 1, GetRowHeight( nRow, nRow ) + 1 );
        const tools::Rectangle aRect(aPoint, aSize);

        return vcl::unotools::b2DRectangleFromRectangle(aRect);
    }
}

// return output range of given row/col range in logical coordinates
basegfx::B2DRange Array::GetB2DRange(sal_Int32 nFirstCol, sal_Int32 nFirstRow, sal_Int32 nLastCol, sal_Int32 nLastRow) const
{
    const Point aPoint( GetColPosition( nFirstCol ), GetRowPosition( nFirstRow ) );
    const Size aSize( GetColWidth( nFirstCol, nLastCol ) + 1, GetRowHeight( nFirstRow, nLastRow ) + 1 );

    return vcl::unotools::b2DRectangleFromRectangle(tools::Rectangle(aPoint, aSize));
}

// mirroring
void Array::MirrorSelfX()
{
    CellVec aNewCells;
    aNewCells.reserve( GetCellCount() );

    size_t nCol, nRow;
    for( nRow = 0; nRow < mxImpl->mnHeight; ++nRow )
    {
        for( nCol = 0; nCol < mxImpl->mnWidth; ++nCol )
        {
            aNewCells.push_back( CELL( mxImpl->GetMirrorCol( nCol ), nRow ) );
            aNewCells.back().MirrorSelfX();
        }
    }
    for( nRow = 0; nRow < mxImpl->mnHeight; ++nRow )
    {
        for( nCol = 0; nCol < mxImpl->mnWidth; ++nCol )
        {
            if( CELL( nCol, nRow ).mbMergeOrig )
            {
                size_t nLastCol = mxImpl->GetMergedLastCol( nCol, nRow );
                size_t nLastRow = mxImpl->GetMergedLastRow( nCol, nRow );
                lclSetMergedRange( aNewCells, mxImpl->mnWidth,
                    mxImpl->GetMirrorCol( nLastCol ), nRow,
                    mxImpl->GetMirrorCol( nCol ), nLastRow );
            }
        }
    }
    mxImpl->maCells.swap( aNewCells );

    std::reverse( mxImpl->maWidths.begin(), mxImpl->maWidths.end() );
    mxImpl->mbXCoordsDirty = true;
}

// drawing
static void HelperCreateHorizontalEntry(
    const Array& rArray,
    const Style& rStyle,
    size_t col,
    size_t row,
    const basegfx::B2DPoint& rOrigin,
    const basegfx::B2DVector& rX,
    const basegfx::B2DVector& rY,
    drawinglayer::primitive2d::SdrFrameBorderDataVector& rData,
    bool bUpper,
    const Color* pForceColor)
{
    // prepare SdrFrameBorderData
    rData.emplace_back(
        bUpper ? rOrigin : basegfx::B2DPoint(rOrigin + rY),
        rX,
        rStyle,
        pForceColor);
    drawinglayer::primitive2d::SdrFrameBorderData& rInstance(rData.back());

    // get involved styles at start
    const Style& rStartFromTR(rArray.GetCellStyleBL( col, row - 1 ));
    const Style& rStartLFromT(rArray.GetCellStyleLeft( col, row - 1 ));
    const Style& rStartLFromL(rArray.GetCellStyleTop( col - 1, row ));
    const Style& rStartLFromB(rArray.GetCellStyleLeft( col, row ));
    const Style& rStartFromBR(rArray.GetCellStyleTL( col, row ));

    rInstance.addSdrConnectStyleData(true, rStartFromTR, rX - rY, false);
    rInstance.addSdrConnectStyleData(true, rStartLFromT, -rY, true);
    rInstance.addSdrConnectStyleData(true, rStartLFromL, -rX, true);
    rInstance.addSdrConnectStyleData(true, rStartLFromB, rY, false);
    rInstance.addSdrConnectStyleData(true, rStartFromBR, rX + rY, false);

    // get involved styles at end
    const Style& rEndFromTL(rArray.GetCellStyleBR( col, row - 1 ));
    const Style& rEndRFromT(rArray.GetCellStyleRight( col, row - 1 ));
    const Style& rEndRFromR(rArray.GetCellStyleTop( col + 1, row ));
    const Style& rEndRFromB(rArray.GetCellStyleRight( col, row ));
    const Style& rEndFromBL(rArray.GetCellStyleTR( col, row ));

    rInstance.addSdrConnectStyleData(false, rEndFromTL, -rX - rY, true);
    rInstance.addSdrConnectStyleData(false, rEndRFromT, -rY, true);
    rInstance.addSdrConnectStyleData(false, rEndRFromR, rX, false);
    rInstance.addSdrConnectStyleData(false, rEndRFromB, rY, false);
    rInstance.addSdrConnectStyleData(false, rEndFromBL, rY - rX, true);
}

static void HelperCreateVerticalEntry(
    const Array& rArray,
    const Style& rStyle,
    size_t col,
    size_t row,
    const basegfx::B2DPoint& rOrigin,
    const basegfx::B2DVector& rX,
    const basegfx::B2DVector& rY,
    drawinglayer::primitive2d::SdrFrameBorderDataVector& rData,
    bool bLeft,
    const Color* pForceColor)
{
    // prepare SdrFrameBorderData
    rData.emplace_back(
        bLeft ? rOrigin : basegfx::B2DPoint(rOrigin + rX),
        rY,
        rStyle,
        pForceColor);
    drawinglayer::primitive2d::SdrFrameBorderData& rInstance(rData.back());

    // get involved styles at start
    const Style& rStartFromBL(rArray.GetCellStyleTR( col - 1, row ));
    const Style& rStartTFromL(rArray.GetCellStyleTop( col - 1, row ));
    const Style& rStartTFromT(rArray.GetCellStyleLeft( col, row - 1 ));
    const Style& rStartTFromR(rArray.GetCellStyleTop( col, row ));
    const Style& rStartFromBR(rArray.GetCellStyleTL( col, row ));

    rInstance.addSdrConnectStyleData(true, rStartFromBR, rX + rY, false);
    rInstance.addSdrConnectStyleData(true, rStartTFromR, rX, false);
    rInstance.addSdrConnectStyleData(true, rStartTFromT, -rY, true);
    rInstance.addSdrConnectStyleData(true, rStartTFromL, -rX, true);
    rInstance.addSdrConnectStyleData(true, rStartFromBL, rY - rX, true);

    // get involved styles at end
    const Style& rEndFromTL(rArray.GetCellStyleBR( col - 1, row ));
    const Style& rEndBFromL(rArray.GetCellStyleBottom( col - 1, row ));
    const Style& rEndBFromB(rArray.GetCellStyleLeft( col, row + 1 ));
    const Style& rEndBFromR(rArray.GetCellStyleBottom( col, row ));
    const Style& rEndFromTR(rArray.GetCellStyleBL( col, row ));

    rInstance.addSdrConnectStyleData(false, rEndFromTR, rX - rY, false);
    rInstance.addSdrConnectStyleData(false, rEndBFromR, rX, false);
    rInstance.addSdrConnectStyleData(false, rEndBFromB, rY, false);
    rInstance.addSdrConnectStyleData(false, rEndBFromL, -rX, true);
    rInstance.addSdrConnectStyleData(false, rEndFromTL, -rY - rX, true);
}

static void HelperClipLine(
    basegfx::B2DPoint& rStart,
    basegfx::B2DVector& rDirection,
    const basegfx::B2DRange& rClipRange)
{
    basegfx::B2DPolygon aLine({rStart, rStart + rDirection});
    const basegfx::B2DPolyPolygon aResultPP(
        basegfx::utils::clipPolygonOnRange(
            aLine,
            rClipRange,
            true, // bInside
            true)); // bStroke

    if(aResultPP.count() > 0)
    {
        const basegfx::B2DPolygon aResultP(aResultPP.getB2DPolygon(0));

        if(aResultP.count() > 0)
        {
            const basegfx::B2DPoint aResultStart(aResultP.getB2DPoint(0));
            const basegfx::B2DPoint aResultEnd(aResultP.getB2DPoint(aResultP.count() - 1));

            if(aResultStart != aResultEnd)
            {
                rStart = aResultStart;
                rDirection = aResultEnd - aResultStart;
            }
        }
    }
}

static void HelperCreateTLBREntry(
    const Array& rArray,
    const Style& rStyle,
    drawinglayer::primitive2d::SdrFrameBorderDataVector& rData,
    const basegfx::B2DPoint& rOrigin,
    const basegfx::B2DVector& rX,
    const basegfx::B2DVector& rY,
    size_t nColLeft,
    size_t nColRight,
    size_t nRowTop,
    size_t nRowBottom,
    const Color* pForceColor,
    const basegfx::B2DRange* pClipRange)
{
    if(rStyle.IsUsed())
    {
        /// prepare geometry line data
        basegfx::B2DPoint aStart(rOrigin);
        basegfx::B2DVector aDirection(rX + rY);

        /// check if we need to clip geometry line data and do it
        if(nullptr != pClipRange)
        {
            HelperClipLine(aStart, aDirection, *pClipRange);
        }

        /// top-left and bottom-right Style Tables
        rData.emplace_back(
            aStart,
            aDirection,
            rStyle,
            pForceColor);
        drawinglayer::primitive2d::SdrFrameBorderData& rInstance(rData.back());

        /// Fill top-left Style Table
        const Style& rTLFromRight(rArray.GetCellStyleTop(nColLeft, nRowTop));
        const Style& rTLFromBottom(rArray.GetCellStyleLeft(nColLeft, nRowTop));

        rInstance.addSdrConnectStyleData(true, rTLFromRight, rX, false);
        rInstance.addSdrConnectStyleData(true, rTLFromBottom, rY, false);

        /// Fill bottom-right Style Table
        const Style& rBRFromBottom(rArray.GetCellStyleRight(nColRight, nRowBottom));
        const Style& rBRFromLeft(rArray.GetCellStyleBottom(nColRight, nRowBottom));

        rInstance.addSdrConnectStyleData(false, rBRFromBottom, -rY, true);
        rInstance.addSdrConnectStyleData(false, rBRFromLeft, -rX, true);
    }
}

static void HelperCreateBLTREntry(
    const Array& rArray,
    const Style& rStyle,
    drawinglayer::primitive2d::SdrFrameBorderDataVector& rData,
    const basegfx::B2DPoint& rOrigin,
    const basegfx::B2DVector& rX,
    const basegfx::B2DVector& rY,
    size_t nColLeft,
    size_t nColRight,
    size_t nRowTop,
    size_t nRowBottom,
    const Color* pForceColor,
    const basegfx::B2DRange* pClipRange)
{
    if(rStyle.IsUsed())
    {
        /// prepare geometry line data
        basegfx::B2DPoint aStart(rOrigin + rY);
        basegfx::B2DVector aDirection(rX - rY);

        /// check if we need to clip geometry line data and do it
        if(nullptr != pClipRange)
        {
            HelperClipLine(aStart, aDirection, *pClipRange);
        }

        /// bottom-left and top-right Style Tables
        rData.emplace_back(
            aStart,
            aDirection,
            rStyle,
            pForceColor);
        drawinglayer::primitive2d::SdrFrameBorderData& rInstance(rData.back());

        /// Fill bottom-left Style Table
        const Style& rBLFromTop(rArray.GetCellStyleLeft(nColLeft, nRowBottom));
        const Style& rBLFromBottom(rArray.GetCellStyleBottom(nColLeft, nRowBottom));

        rInstance.addSdrConnectStyleData(true, rBLFromTop, -rY, true);
        rInstance.addSdrConnectStyleData(true, rBLFromBottom, rX, false);

        /// Fill top-right Style Table
        const Style& rTRFromLeft(rArray.GetCellStyleTop(nColRight, nRowTop));
        const Style& rTRFromBottom(rArray.GetCellStyleRight(nColRight, nRowTop));

        rInstance.addSdrConnectStyleData(false, rTRFromLeft, -rX, true);
        rInstance.addSdrConnectStyleData(false, rTRFromBottom, rY, false);
    }
}

drawinglayer::primitive2d::Primitive2DContainer Array::CreateB2DPrimitiveRange(
    size_t nFirstCol, size_t nFirstRow, size_t nLastCol, size_t nLastRow,
    const Color* pForceColor ) const
{
    DBG_FRAME_CHECK_COLROW( nFirstCol, nFirstRow, "CreateB2DPrimitiveRange" );
    DBG_FRAME_CHECK_COLROW( nLastCol, nLastRow, "CreateB2DPrimitiveRange" );

#ifdef OPTICAL_CHECK_CLIPRANGE_FOR_MERGED_CELL
    std::vector<basegfx::B2DRange> aClipRanges;
#endif

    // It may be necessary to extend the loop ranges by one cell to the outside,
    // when possible. This is needed e.g. when there is in Calc a Cell with an
    // upper CellBorder using DoubleLine and that is right/left connected upwards
    // to also DoubleLine. These upper DoubleLines will be extended to meet the
    // lower of the upper CellBorder and thus have graphical parts that are
    // displayed one cell below and right/left of the target cell - analog to
    // other examples in all other directions.
    // It would be possible to explicitly test this (if possible by indices at all)
    // looping and testing the styles in the outer cells to detect this, but since
    // for other usages (e.g. UI) usually nFirstRow==0 and nLastRow==GetRowCount()-1
    // (and analog for Col) it is okay to just expand the range when available.
    // Do *not* change nFirstRow/nLastRow due to these needed to the boolean tests
    // below (!)
    // Checked usages, this method is used in Calc EditView/Print/Export stuff and
    // in UI (Dialog), not for Writer Tables and Draw/Impress tables. All usages
    // seem okay with this change, so I will add it.
    const size_t nStartRow(nFirstRow > 0 ? nFirstRow - 1 : nFirstRow);
    const size_t nEndRow(nLastRow < GetRowCount() - 1 ? nLastRow + 1 : nLastRow);
    const size_t nStartCol(nFirstCol > 0 ? nFirstCol - 1 : nFirstCol);
    const size_t nEndCol(nLastCol < GetColCount() - 1 ? nLastCol + 1 : nLastCol);

    // prepare SdrFrameBorderDataVector
    std::shared_ptr<drawinglayer::primitive2d::SdrFrameBorderDataVector> aData(
        std::make_shared<drawinglayer::primitive2d::SdrFrameBorderDataVector>());

    // remember for which merged cells crossed lines were already created. To
    // do so, hold the size_t cell index in a set for fast check
    std::set< size_t > aMergedCells;

    for (size_t nRow(nStartRow); nRow <= nEndRow; ++nRow)
    {
        for (size_t nCol(nStartCol); nCol <= nEndCol; ++nCol)
        {
            // get Cell and CoordinateSystem (*only* for this Cell, do *not* expand for
            // merged cells (!)), check if used (non-empty vectors)
            const Cell& rCell(CELL(nCol, nRow));
            basegfx::B2DHomMatrix aCoordinateSystem(rCell.CreateCoordinateSystemSingleCell(*this, nCol, nRow));
            basegfx::B2DVector aX(basegfx::utils::getColumn(aCoordinateSystem, 0));
            basegfx::B2DVector aY(basegfx::utils::getColumn(aCoordinateSystem, 1));

            // get needed local values
            basegfx::B2DPoint aOrigin(basegfx::utils::getColumn(aCoordinateSystem, 2));
            const bool bOverlapX(rCell.mbOverlapX);
            const bool bFirstCol(nCol == nFirstCol);

            // handle rotation: If cell is rotated, handle lower/right edge inside
            // this local geometry due to the created CoordinateSystem already representing
            // the needed transformations.
            const bool bRotated(rCell.IsRotated());

            // Additionally avoid double-handling by suppressing handling when self not rotated,
            // but above/left is rotated and thus already handled. Two directly connected
            // rotated will paint/create both edges, they might be rotated differently.
            const bool bSuppressLeft(!bRotated && nCol > nFirstCol && CELL(nCol - 1, nRow).IsRotated());
            const bool bSuppressAbove(!bRotated && nRow > nFirstRow && CELL(nCol, nRow - 1).IsRotated());

            if(!aX.equalZero() && !aY.equalZero())
            {
                // additionally needed local values
                const bool bOverlapY(rCell.mbOverlapY);
                const bool bLastCol(nCol == nLastCol);
                const bool bFirstRow(nRow == nFirstRow);
                const bool bLastRow(nRow == nLastRow);

                // create upper line for this Cell
                if ((!bOverlapY         // true for first line in merged cells or cells
                    || bFirstRow)       // true for non_Calc usages of this tooling
                    && !bSuppressAbove) // true when above is not rotated, so edge is already handled (see bRotated)
                {
                    // get CellStyle - method will take care to get the correct one, e.g.
                    // for merged cells (it uses ORIGCELL that works with topLeft's of these)
                    const Style& rTop(GetCellStyleTop(nCol, nRow));

                    if(rTop.IsUsed())
                    {
                        HelperCreateHorizontalEntry(*this, rTop, nCol, nRow, aOrigin, aX, aY, *aData, true, pForceColor);
                    }
                }

                // create lower line for this Cell
                if (bLastRow       // true for non_Calc usages of this tooling
                    || bRotated)   // true if cell is rotated, handle lower edge in local geometry
                {
                    const Style& rBottom(GetCellStyleBottom(nCol, nRow));

                    if(rBottom.IsUsed())
                    {
                        HelperCreateHorizontalEntry(*this, rBottom, nCol, nRow + 1, aOrigin, aX, aY, *aData, false, pForceColor);
                    }
                }

                // create left line for this Cell
                if ((!bOverlapX         // true for first column in merged cells or cells
                    || bFirstCol)       // true for non_Calc usages of this tooling
                    && !bSuppressLeft)  // true when left is not rotated, so edge is already handled (see bRotated)
                {
                    const Style& rLeft(GetCellStyleLeft(nCol, nRow));

                    if(rLeft.IsUsed())
                    {
                        HelperCreateVerticalEntry(*this, rLeft, nCol, nRow, aOrigin, aX, aY, *aData, true, pForceColor);
                    }
                }

                // create right line for this Cell
                if (bLastCol        // true for non_Calc usages of this tooling
                    || bRotated)    // true if cell is rotated, handle right edge in local geometry
                {
                    const Style& rRight(GetCellStyleRight(nCol, nRow));

                    if(rRight.IsUsed())
                    {
                        HelperCreateVerticalEntry(*this, rRight, nCol + 1, nRow, aOrigin, aX, aY, *aData, false, pForceColor);
                    }
                }

                // tdf#126269 check for crossed lines, these need special treatment, especially
                // for merged cells (see comments in task). Separate treatment of merged and
                // non-merged cells to allow better handling of both types
                if(rCell.IsMerged())
                {
                    // first check if this merged cell was already handled. To do so,
                    // calculate and use the index of the TopLeft cell
                    size_t nColLeft(nCol), nRowTop(nRow), nColRight(nCol), nRowBottom(nRow);
                    GetMergedRange(nColLeft, nRowTop, nColRight, nRowBottom, nCol, nRow);
                    const size_t nIndexOfMergedCell(mxImpl->GetIndex(nColLeft, nRowTop));

                    if(aMergedCells.end() == aMergedCells.find(nIndexOfMergedCell))
                    {
                        // not found, so not yet handled. Add now to mark as handled
                        aMergedCells.insert(nIndexOfMergedCell);

                        // Get and check if diagonal styles are used
                        // Note: For GetCellStyleBLTR below I tried to use nRowBottom
                        //       as Y-value what seemed more logical, but that
                        //       is wrong. Despite defining a line starting at
                        //       bottom-left, the Style is defined in the cell at top-left
                        const Style& rTLBR(GetCellStyleTLBR(nColLeft, nRowTop));
                        const Style& rBLTR(GetCellStyleBLTR(nColLeft, nRowTop));

                        if(rTLBR.IsUsed() || rBLTR.IsUsed())
                        {
                            // test if merged cell overlaps ClipRange at all (needs visualization)
                            if(mxImpl->OverlapsClipRange(nColLeft, nRowTop, nColRight, nRowBottom))
                            {
                                // when merged, get extended coordinate system and derived values
                                // for the full range of this merged cell. Only work with rMergedCell
                                // (which is the top-left single cell of the merged cell) from here on
                                const Cell& rMergedCell(CELL(nColLeft, nRowTop));
                                aCoordinateSystem = rMergedCell.CreateCoordinateSystemMergedCell(
                                    *this, nColLeft, nRowTop, nColRight, nRowBottom);
                                aX = basegfx::utils::getColumn(aCoordinateSystem, 0);
                                aY = basegfx::utils::getColumn(aCoordinateSystem, 1);
                                aOrigin = basegfx::utils::getColumn(aCoordinateSystem, 2);

                                // check if clip is needed
                                basegfx::B2DRange aClipRange;

                                // first use row/col ClipTest for raw check
                                bool bNeedToClip(
                                    !mxImpl->IsColInClipRange(nColLeft) ||
                                    !mxImpl->IsRowInClipRange(nRowTop) ||
                                    !mxImpl->IsColInClipRange(nColRight) ||
                                    !mxImpl->IsRowInClipRange(nRowBottom));

                                if(bNeedToClip)
                                {
                                    // now get ClipRange and CellRange in logical coordinates
                                    aClipRange = GetB2DRange(
                                        mxImpl->mnFirstClipCol, mxImpl->mnFirstClipRow,
                                        mxImpl->mnLastClipCol, mxImpl->mnLastClipRow);

                                    basegfx::B2DRange aCellRange(
                                        GetB2DRange(
                                            nColLeft, nRowTop,
                                            nColRight, nRowBottom));

                                    // intersect these to get the target ClipRange, ensure
                                    // that clip is needed
                                    aClipRange.intersect(aCellRange);
                                    bNeedToClip = !aClipRange.isEmpty();

#ifdef OPTICAL_CHECK_CLIPRANGE_FOR_MERGED_CELL
                                    aClipRanges.push_back(aClipRange);
#endif
                                }

                                // create top-left to bottom-right geometry
                                HelperCreateTLBREntry(*this, rTLBR, *aData, aOrigin, aX, aY,
                                    nColLeft, nRowTop, nColRight, nRowBottom, pForceColor,
                                    bNeedToClip ? &aClipRange : nullptr);

                                // create bottom-left to top-right geometry
                                HelperCreateBLTREntry(*this, rBLTR, *aData, aOrigin, aX, aY,
                                    nColLeft, nRowTop, nColRight, nRowBottom, pForceColor,
                                    bNeedToClip ? &aClipRange : nullptr);
                            }
                        }
                    }
                }
                else
                {
                    // must be in clipping range: else not visible. This
                    // already clips completely for non-merged cells
                    if( mxImpl->IsInClipRange( nCol, nRow ) )
                    {
                        // get and check if diagonal styles are used
                        const Style& rTLBR(GetCellStyleTLBR(nCol, nRow));
                        const Style& rBLTR(GetCellStyleBLTR(nCol, nRow));

                        if(rTLBR.IsUsed() || rBLTR.IsUsed())
                        {
                            HelperCreateTLBREntry(*this, rTLBR, *aData, aOrigin, aX, aY,
                                nCol, nRow, nCol, nRow, pForceColor, nullptr);

                            HelperCreateBLTREntry(*this, rBLTR, *aData, aOrigin, aX, aY,
                                nCol, nRow, nCol, nRow, pForceColor, nullptr);
                        }
                    }
                }
            }
            else if(!aY.equalZero())
            {
                // cell has height, but no width. Create left vertical line for this Cell
                if ((!bOverlapX         // true for first column in merged cells or cells
                    || bFirstCol)       // true for non_Calc usages of this tooling
                    && !bSuppressLeft)  // true when left is not rotated, so edge is already handled (see bRotated)
                {
                    const Style& rLeft(GetCellStyleLeft(nCol, nRow));

                    if (rLeft.IsUsed())
                    {
                        HelperCreateVerticalEntry(*this, rLeft, nCol, nRow, aOrigin, aX, aY, *aData, true, pForceColor);
                    }
                }
            }
            else
            {
                // Cell has *no* size, thus no visualization
            }
        }
    }

    // create instance of SdrFrameBorderPrimitive2D if
    // SdrFrameBorderDataVector is used
    drawinglayer::primitive2d::Primitive2DContainer aSequence;

    if(!aData->empty())
    {
        aSequence.append(
            drawinglayer::primitive2d::Primitive2DReference(
                new drawinglayer::primitive2d::SdrFrameBorderPrimitive2D(
                    aData,
                    true)));    // force visualization to minimal one discrete unit (pixel)
    }

#ifdef OPTICAL_CHECK_CLIPRANGE_FOR_MERGED_CELL
    for(auto const& rClipRange : aClipRanges)
    {
        // draw ClipRange in yellow to allow simple interactive optical control in office
        aSequence.append(
            drawinglayer::primitive2d::Primitive2DReference(
                new drawinglayer::primitive2d::PolygonHairlinePrimitive2D(
                    basegfx::utils::createPolygonFromRect(rClipRange),
                    basegfx::BColor(1.0, 1.0, 0.0))));
    }
#endif

    return aSequence;
}

drawinglayer::primitive2d::Primitive2DContainer Array::CreateB2DPrimitiveArray() const
{
    drawinglayer::primitive2d::Primitive2DContainer aPrimitives;

    if (mxImpl->mnWidth && mxImpl->mnHeight)
    {
        aPrimitives = CreateB2DPrimitiveRange(0, 0, mxImpl->mnWidth - 1, mxImpl->mnHeight - 1, nullptr);
    }

    return aPrimitives;
}

#undef ORIGCELL
#undef LASTCELL
#undef CELLACC
#undef CELL
#undef DBG_FRAME_CHECK_ROW_1
#undef DBG_FRAME_CHECK_COL_1
#undef DBG_FRAME_CHECK_COLROW
#undef DBG_FRAME_CHECK_ROW
#undef DBG_FRAME_CHECK_COL
#undef DBG_FRAME_CHECK

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

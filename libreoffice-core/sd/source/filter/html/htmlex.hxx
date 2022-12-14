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

#include <resltn.hxx>
#include <rtl/ustrbuf.hxx>
#include <tools/color.hxx>
#include <tools/solar.h>
#include <vcl/errinf.hxx>
#include <unotools/resmgr.hxx>

#include "htmlpublishmode.hxx"

#include <memory>
#include <string_view>
#include <vector>

namespace basegfx { class B2DPolyPolygon; }
namespace com::sun::star::beans { struct PropertyValue; }
namespace com::sun::star::ucb { class XSimpleFileAccess3; }
namespace sd { class DrawDocShell; }
namespace tools { class Rectangle; }

#define PUB_LOWRES_WIDTH    640
#define PUB_MEDRES_WIDTH    800
#define PUB_HIGHRES_WIDTH   1024
#define PUB_FHDRES_WIDTH    1920

#define PUB_THUMBNAIL_WIDTH  256
#define PUB_THUMBNAIL_HEIGHT 192

class ErrCode;
class OutlinerParaObject;
class SfxItemSet;
class Size;
class SfxProgress;
class SdrOutliner;
class SdPage;
class HtmlState;
class SdrTextObj;
class SdrObjGroup;
namespace sdr::table { class SdrTableObj; }
class SdrPage;
class SdDrawDocument;
class ButtonSet;

class HtmlErrorContext : public ErrorContext
{
private:
    TranslateId mpResId;
    OUString  maURL1;
    OUString  maURL2;

public:
                    explicit HtmlErrorContext();

    virtual bool    GetString( ErrCode nErrId, OUString& rCtxStr ) override;

    void            SetContext(TranslateId pResId, const OUString& rURL);
    void            SetContext(TranslateId pResId, const OUString& rURL1, const OUString& rURL2);
};

/// this class exports an Impress Document as a HTML Presentation.
class HtmlExport final
{
    std::vector< SdPage* > maPages;
    std::vector< SdPage* > maNotesPages;

    OUString maPath;

    SdDrawDocument* mpDoc;
    ::sd::DrawDocShell* mpDocSh;

    HtmlErrorContext meEC;

    HtmlPublishMode meMode;
    std::unique_ptr<SfxProgress> mpProgress;
    bool mbImpress;
    sal_uInt16 mnSdPageCount;
    sal_uInt16 mnPagesWritten;
    bool mbContentsPage;
    sal_Int16 mnButtonThema;
    sal_uInt16 mnWidthPixel;
    sal_uInt16 mnHeightPixel;
    PublishingFormat meFormat;
    bool mbHeader;
    bool mbNotes;
    bool mbFrames;
    OUString maIndex;
    OUString maEMail;
    OUString maAuthor;
    OUString maHomePage;
    OUString maInfo;
    sal_Int16 mnCompression;
    OUString maDocFileName;
    OUString maFramePage;
    OUString mDocTitle;
    bool mbDownload;

    bool mbAutoSlide;
    double  mfSlideDuration;
    bool mbSlideSound;
    bool mbHiddenSlides;
    bool mbEndless;

    bool mbUserAttr;
    Color maTextColor; ///< The following colors are used for the <body> tag if mbUserAttr is true.
    Color maBackColor;
    Color maLinkColor;
    Color maVLinkColor;
    Color maALinkColor;
    Color maFirstPageColor;
    bool mbDocColors;

    std::vector<OUString> maHTMLFiles;
    std::vector<OUString> maImageFiles;
    std::vector<OUString> maThumbnailFiles;
    std::vector<OUString> maPageNames;
    std::vector<OUString> maTextFiles;

    OUString maExportPath; ///< output directory or URL.
    OUString maIndexUrl;
    OUString maURLPath;
    OUString maCGIPath;
    PublishingScript meScript;

    std::unique_ptr< ButtonSet > mpButtonSet;

    static SdrTextObj* GetLayoutTextObject(SdrPage const * pPage);

    void SetDocColors( SdPage* pPage = nullptr );

    bool        CreateImagesForPresPages( bool bThumbnails = false );
    bool    CreateHtmlTextForPresPages();
    bool    CreateHtmlForPresPages();
    bool    CreateContentPage();
    void    CreateFileNames();
    void    CreateBitmaps();
    bool    CreateOutlinePages();
    bool    CreateFrames();
    bool    CreateNotesPages();
    bool    CreateNavBarFrames();

    bool    CreateASPScripts();
    bool    CreatePERLScripts();
    bool    CreateImageFileList();
    bool    CreateImageNumberFile();

    bool    checkForExistingFiles();
    bool    checkFileExists( css::uno::Reference< css::ucb::XSimpleFileAccess3 > const & xFileAccess, std::u16string_view aFileName );

    OUString const & getDocumentTitle();
    bool    SavePresentation();

    static OUString CreateLink( std::u16string_view aLink, std::u16string_view aText,
                        std::u16string_view aTarget = std::u16string_view());
    static OUString CreateImage( std::u16string_view aImage, std::u16string_view aAltText );
    OUString CreateNavBar( sal_uInt16 nSdPage, bool bIsText ) const;
    OUString CreateBodyTag() const;

    OUString ParagraphToHTMLString( SdrOutliner const * pOutliner, sal_Int32 nPara, const Color& rBackgroundColor );
    OUString TextAttribToHTMLString( SfxItemSet const * pSet, HtmlState* pState, const Color& rBackgroundColor );

    OUString CreateTextForTitle( SdrOutliner* pOutliner, SdPage* pPage, const Color& rBackgroundColor );
    OUString CreateTextForPage( SdrOutliner* pOutliner, SdPage const * pPage, bool bHeadLine, const Color& rBackgroundColor );
    OUString CreateTextForNotesPage( SdrOutliner* pOutliner, SdPage* pPage, const Color& rBackgroundColor );

    static OUString CreateHTMLCircleArea( sal_uLong nRadius, sal_uLong nCenterX,
                                  sal_uLong nCenterY, std::u16string_view rHRef );
    static OUString CreateHTMLPolygonArea( const ::basegfx::B2DPolyPolygon& rPolyPoly, Size aShift, double fFactor, std::u16string_view rHRef );
    static OUString CreateHTMLRectArea( const ::tools::Rectangle& rRect,
                                std::u16string_view rHRef );

    OUString CreatePageURL( sal_uInt16 nPgNum );

    OUString InsertSound( const OUString& rSoundFile );
    bool CopyFile( const OUString& rSourceFile, const OUString& rDestFile );
    bool CopyScript( std::u16string_view rPath, const OUString& rSource, const OUString& rDest, bool bUnix = false );

    void InitProgress( sal_uInt16 nProgrCount );
    void ResetProgress();

    /// Output only the charset metadata, title etc. will be handled separately.
    static OUString CreateMetaCharset();

    /// Output document metadata.
    OUString DocumentMetadata() const;

    void InitExportParameters( const css::uno::Sequence< css::beans::PropertyValue >& rParams);
    void ExportHtml();
    void ExportKiosk();
    void ExportWebCast();
    void ExportSingleDocument();

    bool WriteHtml( const OUString& rFileName, bool bAddExtension, std::u16string_view rHtmlData );
    static OUString GetButtonName( int nButton );

    void WriteOutlinerParagraph(OUStringBuffer& aStr, SdrOutliner* pOutliner,
                                OutlinerParaObject const * pOutlinerParagraphObject,
                                const Color& rBackgroundColor, bool bHeadLine);

    void WriteObjectGroup(OUStringBuffer& aStr, SdrObjGroup const * pObjectGroup,
                          SdrOutliner* pOutliner, const Color& rBackgroundColor, bool bHeadLine);

    void WriteTable(OUStringBuffer& aStr, sdr::table::SdrTableObj const * pTableObject,
                    SdrOutliner* pOutliner, const Color& rBackgroundColor);

 public:
    HtmlExport(const OUString& aPath,
               const css::uno::Sequence<css::beans::PropertyValue>& rParams,
               SdDrawDocument* pExpDoc,
               sd::DrawDocShell* pDocShell);

    ~HtmlExport();

    static OUString ColorToHTMLString( Color aColor );
    static OUString StringToHTMLString( const OUString& rString );
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

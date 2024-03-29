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

#include "IDocumentOutlineNodes.hxx"
#include "IDocumentRedlineAccess.hxx"
#include "IDocumentTimerAccess.hxx"
#include "colorizer.hxx"
#include "itabenum.hxx"
#include "ndtxt.hxx"
#include "redline.hxx"
#include "swundo.hxx"
#include "txtfrm.hxx"
#include "wrtsh.hxx"
#include <iostream>
#include <unotxdoc.hxx>

#include <map>
#include <utility>
#include <vector>

#include <com/sun/star/beans/XPropertyAccess.hpp>

#include <comphelper/sequence.hxx>
#include <o3tl/string_view.hxx>
#include <tools/json_writer.hxx>
#include <tools/urlobj.hxx>
#include <xmloff/odffields.hxx>

#include <IDocumentMarkAccess.hxx>
#include <doc.hxx>
#include <docsh.hxx>
#include <fmtrfmrk.hxx>
#include <txtrfmrk.hxx>
#include <ndtxt.hxx>
#include <wrtsh.hxx>
#include <swmodule.hxx>
#include <view.hxx>
#include <docsh.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <docfld.hxx>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace ::com::sun::star;

namespace
{
/// Implements getCommandValues(".uno:TextFormFields").
///
/// Parameters:
///
/// - type: e.g. ODF_UNHANDLED
/// - commandPrefix: field command prefix to not return all fieldmarks
void GetTextFormFields(tools::JsonWriter& rJsonWriter, SwDocShell* pDocShell,
                       const std::map<OUString, OUString>& rArguments)
{
    OUString aType;
    OUString aCommandPrefix;
    {
        auto it = rArguments.find("type");
        if (it != rArguments.end())
        {
            aType = it->second;
        }

        it = rArguments.find("commandPrefix");
        if (it != rArguments.end())
        {
            aCommandPrefix = it->second;
        }
    }

    SwDoc* pDoc = pDocShell->GetDoc();
    IDocumentMarkAccess* pMarkAccess = pDoc->getIDocumentMarkAccess();
    tools::ScopedJsonWriterArray aFields = rJsonWriter.startArray("fields");
    for (auto it = pMarkAccess->getFieldmarksBegin(); it != pMarkAccess->getFieldmarksEnd(); ++it)
    {
        auto pFieldmark = dynamic_cast<sw::mark::IFieldmark*>(*it);
        assert(pFieldmark);
        if (pFieldmark->GetFieldname() != aType)
        {
            continue;
        }

        auto itParam = pFieldmark->GetParameters()->find(ODF_CODE_PARAM);
        if (itParam == pFieldmark->GetParameters()->end())
        {
            continue;
        }

        OUString aCommand;
        itParam->second >>= aCommand;
        if (!aCommand.startsWith(aCommandPrefix))
        {
            continue;
        }

        tools::ScopedJsonWriterStruct aField = rJsonWriter.startStruct();
        rJsonWriter.put("type", aType);
        rJsonWriter.put("command", aCommand);
    }
}

/// Implements getCommandValues(".uno:TextFormField").
///
/// Parameters:
///
/// - type: e.g. ODF_UNHANDLED
/// - commandPrefix: field command prefix to not return all fieldmarks
void GetTextFormField(tools::JsonWriter& rJsonWriter, SwDocShell* pDocShell,
                      const std::map<OUString, OUString>& rArguments)
{
    OUString aType;
    OUString aCommandPrefix;
    auto it = rArguments.find("type");
    if (it != rArguments.end())
    {
        aType = it->second;
    }

    it = rArguments.find("commandPrefix");
    if (it != rArguments.end())
    {
        aCommandPrefix = it->second;
    }

    IDocumentMarkAccess& rIDMA = *pDocShell->GetDoc()->getIDocumentMarkAccess();
    SwWrtShell* pWrtShell = pDocShell->GetWrtShell();
    SwPosition& rCursor = *pWrtShell->GetCursor()->GetPoint();
    sw::mark::IFieldmark* pFieldmark = rIDMA.getFieldmarkFor(rCursor);
    auto typeNode = rJsonWriter.startNode("field");
    if (!pFieldmark)
    {
        return;
    }

    if (pFieldmark->GetFieldname() != aType)
    {
        return;
    }

    auto itParam = pFieldmark->GetParameters()->find(ODF_CODE_PARAM);
    if (itParam == pFieldmark->GetParameters()->end())
    {
        return;
    }

    OUString aCommand;
    itParam->second >>= aCommand;
    if (!aCommand.startsWith(aCommandPrefix))
    {
        return;
    }

    rJsonWriter.put("type", aType);
    rJsonWriter.put("command", aCommand);
}

/// Implements getCommandValues(".uno:SetDocumentProperties").
///
/// Parameters:
///
/// - namePrefix: field name prefix to not return all user-defined properties
void GetDocumentProperties(tools::JsonWriter& rJsonWriter, SwDocShell* pDocShell,
                           const std::map<OUString, OUString>& rArguments)
{
    OUString aNamePrefix;
    auto it = rArguments.find("namePrefix");
    if (it != rArguments.end())
    {
        aNamePrefix = it->second;
    }

    uno::Reference<document::XDocumentPropertiesSupplier> xDPS(pDocShell->GetModel(),
                                                               uno::UNO_QUERY);
    uno::Reference<document::XDocumentProperties> xDP = xDPS->getDocumentProperties();
    uno::Reference<beans::XPropertyAccess> xUDP(xDP->getUserDefinedProperties(), uno::UNO_QUERY);
    auto aUDPs = comphelper::sequenceToContainer<std::vector<beans::PropertyValue>>(
        xUDP->getPropertyValues());
    tools::ScopedJsonWriterArray aProperties = rJsonWriter.startArray("userDefinedProperties");
    for (const auto& rUDP : aUDPs)
    {
        if (!rUDP.Name.startsWith(aNamePrefix))
        {
            continue;
        }

        if (rUDP.Value.getValueTypeClass() != uno::TypeClass_STRING)
        {
            continue;
        }

        OUString aValue;
        rUDP.Value >>= aValue;

        tools::ScopedJsonWriterStruct aProperty = rJsonWriter.startStruct();
        rJsonWriter.put("name", rUDP.Name);
        rJsonWriter.put("type", "string");
        rJsonWriter.put("value", aValue);
    }
}

/// Implements getCommandValues(".uno:Bookmarks").
///
/// Parameters:
///
/// - namePrefix: bookmark name prefix to not return all bookmarks
void GetBookmarks(tools::JsonWriter& rJsonWriter, SwDocShell* pDocShell,
                  const std::map<OUString, OUString>& rArguments)
{
    OUString aNamePrefix;
    {
        auto it = rArguments.find("namePrefix");
        if (it != rArguments.end())
        {
            aNamePrefix = it->second;
        }
    }

    IDocumentMarkAccess& rIDMA = *pDocShell->GetDoc()->getIDocumentMarkAccess();
    tools::ScopedJsonWriterArray aBookmarks = rJsonWriter.startArray("bookmarks");
    for (auto it = rIDMA.getBookmarksBegin(); it != rIDMA.getBookmarksEnd(); ++it)
    {
        sw::mark::IMark* pMark = *it;
        if (!pMark->GetName().startsWith(aNamePrefix))
        {
            continue;
        }

        tools::ScopedJsonWriterStruct aProperty = rJsonWriter.startStruct();
        rJsonWriter.put("name", pMark->GetName());
    }
}

/// Implements getCommandValues(".uno:GetOutline").
void GetOutline(tools::JsonWriter& rJsonWriter, SwDocShell* pDocShell)
{
    SwWrtShell* mrSh = pDocShell->GetWrtShell();

    const SwOutlineNodes::size_type nOutlineCount
        = mrSh->getIDocumentOutlineNodesAccess()->getOutlineNodesCount();

    typedef std::pair<sal_Int8, sal_Int32> StackEntry;
    std::stack<StackEntry> aOutlineStack;
    aOutlineStack.push(StackEntry(-1, -1)); // push default value

    tools::ScopedJsonWriterArray aOutline = rJsonWriter.startArray("outline");

    // Allows for 65535 nodes in the outline
    sal_uInt16 nOutlineId = 0;

    for (SwOutlineNodes::size_type i = 0; i < nOutlineCount; ++i)
    {
        // Check if outline is hidden
        const SwTextNode* textNode = mrSh->GetNodes().GetOutLineNds()[i]->GetTextNode();

        if (textNode->IsHidden() || !sw::IsParaPropsNode(*mrSh->GetLayout(), *textNode) ||
            // Skip empty outlines:
            textNode->GetText().isEmpty())
        {
            nOutlineId++;
            continue;
        }

        // Get parent id from stack:
        const sal_Int8 nLevel
            = static_cast<sal_Int8>(mrSh->getIDocumentOutlineNodesAccess()->getOutlineLevel(i));

        sal_Int8 nLevelOnTopOfStack = aOutlineStack.top().first;
        while (nLevelOnTopOfStack >= nLevel && nLevelOnTopOfStack != -1)
        {
            aOutlineStack.pop();
            nLevelOnTopOfStack = aOutlineStack.top().first;
        }

        const sal_Int32 nParent = aOutlineStack.top().second;

        tools::ScopedJsonWriterStruct aProperty = rJsonWriter.startStruct();

        // We need to explicitly get the text here in case there are special characters
        // Using textNode->GetText() will result in a failure to convert the result to JSON in the DocumentClient::GetCommandValues in electron-libreoffice
        const OUString& rEntry = mrSh->getIDocumentOutlineNodesAccess()->getOutlineText(
            i, mrSh->GetLayout(), true, false, false);

        rJsonWriter.put("id", nOutlineId);
        rJsonWriter.put("parent", nParent);
        rJsonWriter.put("text", rEntry);

        aOutlineStack.push(StackEntry(nLevel, nOutlineId));

        nOutlineId++;
    }
}

/// Implements getCommandValues(".uno:Bookmark").
///
/// Parameters:
///
/// - namePrefix: bookmark name prefix to not return all bookmarks
void GetBookmark(tools::JsonWriter& rJsonWriter, SwDocShell* pDocShell,
                 const std::map<OUString, OUString>& rArguments)
{
    OUString aNamePrefix;
    {
        auto it = rArguments.find("namePrefix");
        if (it != rArguments.end())
        {
            aNamePrefix = it->second;
        }
    }

    IDocumentMarkAccess& rIDMA = *pDocShell->GetDoc()->getIDocumentMarkAccess();
    SwWrtShell* pWrtShell = pDocShell->GetWrtShell();
    SwPosition& rCursor = *pWrtShell->GetCursor()->GetPoint();
    sw::mark::IMark* pBookmark = rIDMA.getBookmarkFor(rCursor);
    tools::ScopedJsonWriterNode aBookmark = rJsonWriter.startNode("bookmark");
    if (!pBookmark)
    {
        return;
    }

    if (!pBookmark->GetName().startsWith(aNamePrefix))
    {
        return;
    }

    rJsonWriter.put("name", pBookmark->GetName());
}

/// Implements getCommandValues(".uno:Fields").
///
/// Parameters:
///
/// - typeName: field type condition to not return all fields
/// - namePrefix: field name prefix to not return all fields
void GetFields(tools::JsonWriter& rJsonWriter, SwDocShell* pDocShell,
               const std::map<OUString, OUString>& rArguments)
{
    OUString aTypeName;
    {
        auto it = rArguments.find("typeName");
        if (it != rArguments.end())
        {
            aTypeName = it->second;
        }
    }
    // See SwFieldTypeFromString().
    if (aTypeName != "SetRef")
    {
        return;
    }

    OUString aNamePrefix;
    {
        auto it = rArguments.find("namePrefix");
        if (it != rArguments.end())
        {
            aNamePrefix = it->second;
        }
    }

    SwDoc* pDoc = pDocShell->GetDoc();
    tools::ScopedJsonWriterArray aBookmarks = rJsonWriter.startArray("setRefs");
    std::vector<const SwFormatRefMark*> aRefMarks;
    for (sal_uInt16 i = 0; i < pDoc->GetRefMarks(); ++i)
    {
        aRefMarks.push_back(pDoc->GetRefMark(i));
    }
    // Sort the refmarks based on their start position.
    std::sort(aRefMarks.begin(), aRefMarks.end(),
              [](const SwFormatRefMark* pMark1, const SwFormatRefMark* pMark2) -> bool
              {
                  const SwTextRefMark* pTextRefMark1 = pMark1->GetTextRefMark();
                  const SwTextRefMark* pTextRefMark2 = pMark2->GetTextRefMark();
                  SwPosition aPos1(pTextRefMark1->GetTextNode(), pTextRefMark1->GetStart());
                  SwPosition aPos2(pTextRefMark2->GetTextNode(), pTextRefMark2->GetStart());
                  return aPos1 < aPos2;
              });

    for (const auto& pRefMark : aRefMarks)
    {
        if (!pRefMark->GetRefName().startsWith(aNamePrefix))
        {
            continue;
        }

        tools::ScopedJsonWriterStruct aProperty = rJsonWriter.startStruct();
        rJsonWriter.put("name", pRefMark->GetRefName());
    }
}

/// Implements getCommandValues(".uno:Field").
///
/// Parameters:
///
/// - typeName: field type condition to not return all fields
/// - namePrefix: field name prefix to not return all fields
void GetField(tools::JsonWriter& rJsonWriter, SwDocShell* pDocShell,
              const std::map<OUString, OUString>& rArguments)
{
    OUString aTypeName;
    {
        auto it = rArguments.find("typeName");
        if (it != rArguments.end())
        {
            aTypeName = it->second;
        }
    }
    // See SwFieldTypeFromString().
    if (aTypeName != "SetRef")
    {
        return;
    }

    OUString aNamePrefix;
    {
        auto it = rArguments.find("namePrefix");
        if (it != rArguments.end())
        {
            aNamePrefix = it->second;
        }
    }

    SwWrtShell* pWrtShell = pDocShell->GetWrtShell();
    SwPosition& rCursor = *pWrtShell->GetCursor()->GetPoint();
    SwTextNode* pTextNode = rCursor.GetNode().GetTextNode();
    std::vector<SwTextAttr*> aAttrs
        = pTextNode->GetTextAttrsAt(rCursor.GetContentIndex(), RES_TXTATR_REFMARK);
    tools::ScopedJsonWriterNode aRefmark = rJsonWriter.startNode("setRef");
    if (aAttrs.empty())
    {
        return;
    }

    const SwFormatRefMark& rRefmark = aAttrs[0]->GetRefMark();
    if (!rRefmark.GetRefName().startsWith(aNamePrefix))
    {
        return;
    }

    rJsonWriter.put("name", rRefmark.GetRefName());
}

/// Implements getCommandValues(".uno:Sections").
///
/// Parameters:
///
/// - namePrefix: field name prefix to not return all sections
void GetSections(tools::JsonWriter& rJsonWriter, SwDocShell* pDocShell,
                 const std::map<OUString, OUString>& rArguments)
{
    OUString aNamePrefix;
    {
        auto it = rArguments.find("namePrefix");
        if (it != rArguments.end())
        {
            aNamePrefix = it->second;
        }
    }

    SwDoc* pDoc = pDocShell->GetDoc();
    tools::ScopedJsonWriterArray aBookmarks = rJsonWriter.startArray("sections");
    for (const auto& pSection : pDoc->GetSections())
    {
        if (!pSection->GetName().startsWith(aNamePrefix))
        {
            continue;
        }

        tools::ScopedJsonWriterStruct aProperty = rJsonWriter.startStruct();
        rJsonWriter.put("name", pSection->GetName());
    }
}
}

bool SwXTextDocument::supportsCommand(std::u16string_view rCommand)
{
    static const std::initializer_list<std::u16string_view> vForward
        = { u"TextFormFields", u"TextFormField", u"SetDocumentProperties",
            u"Bookmarks",      u"Fields",        u"Sections",
            u"Bookmark",       u"Field",         u"GetOutline" };

    return std::find(vForward.begin(), vForward.end(), rCommand) != vForward.end();
}

void SwXTextDocument::getCommandValues(tools::JsonWriter& rJsonWriter, std::string_view rCommand)
{
    std::map<OUString, OUString> aMap;

    static constexpr OStringLiteral aTextFormFields(".uno:TextFormFields");
    static constexpr OStringLiteral aTextFormField(".uno:TextFormField");
    static constexpr OStringLiteral aSetDocumentProperties(".uno:SetDocumentProperties");
    static constexpr OStringLiteral aBookmarks(".uno:Bookmarks");
    static constexpr OStringLiteral aGetOutline(".uno:GetOutline");
    static constexpr OStringLiteral aFields(".uno:Fields");
    static constexpr OStringLiteral aSections(".uno:Sections");
    static constexpr OStringLiteral aBookmark(".uno:Bookmark");
    static constexpr OStringLiteral aField(".uno:Field");

    INetURLObject aParser(OUString::fromUtf8(rCommand));
    OUString aArguments = aParser.GetParam();
    sal_Int32 nParamIndex = 0;
    do
    {
        std::u16string_view aParam = o3tl::getToken(aArguments, 0, '&', nParamIndex);
        sal_Int32 nIndex = 0;
        OUString aKey;
        OUString aValue;
        do
        {
            std::u16string_view aToken = o3tl::getToken(aParam, 0, '=', nIndex);
            if (aKey.isEmpty())
                aKey = aToken;
            else
                aValue = aToken;
        } while (nIndex >= 0);
        OUString aDecodedValue
            = INetURLObject::decode(aValue, INetURLObject::DecodeMechanism::WithCharset);
        aMap[aKey] = aDecodedValue;
    } while (nParamIndex >= 0);

    if (o3tl::starts_with(rCommand, aTextFormFields))
    {
        GetTextFormFields(rJsonWriter, m_pDocShell, aMap);
    }
    if (o3tl::starts_with(rCommand, aTextFormField))
    {
        GetTextFormField(rJsonWriter, m_pDocShell, aMap);
    }
    else if (o3tl::starts_with(rCommand, aSetDocumentProperties))
    {
        GetDocumentProperties(rJsonWriter, m_pDocShell, aMap);
    }
    else if (o3tl::starts_with(rCommand, aBookmarks))
    {
        GetBookmarks(rJsonWriter, m_pDocShell, aMap);
    }
    else if (o3tl::starts_with(rCommand, aGetOutline))
    {
        GetOutline(rJsonWriter, m_pDocShell);
    }
    else if (o3tl::starts_with(rCommand, aFields))
    {
        GetFields(rJsonWriter, m_pDocShell, aMap);
    }
    else if (o3tl::starts_with(rCommand, aSections))
    {
        GetSections(rJsonWriter, m_pDocShell, aMap);
    }
    else if (o3tl::starts_with(rCommand, aBookmark))
    {
        GetBookmark(rJsonWriter, m_pDocShell, aMap);
    }
    else if (o3tl::starts_with(rCommand, aField))
    {
        GetField(rJsonWriter, m_pDocShell, aMap);
    }
}

bool SwXTextDocument::gotoOutline(tools::JsonWriter& rJsonWriter, int idx)
{
    SwWrtShell* mrSh = m_pDocShell->GetWrtShell();

    if (idx < 0) return false;
    if ((size_t)idx >= m_pDocShell->GetDoc()->GetNodes().GetOutLineNds().size()) return false;
    mrSh->GotoOutline(idx);

    SwRect destRect = mrSh->GetCharRect();

    rJsonWriter.put("destRect", destRect.SVRect().toString());
    return true;
}

void SwXTextDocument::createTable(int row, int col)
{
    SwWrtShell* mrSh = m_pDocShell->GetWrtShell();

    const SwInsertTableOptions aInsertTableOptions(SwInsertTableFlags::DefaultBorder,
                                                   /*nRowsToRepeat=*/0);

    mrSh->InsertTable(aInsertTableOptions, row, col);
}

// MACRO-1212: batch track change updates in a single action
void SwXTextDocument::batchUpdateTrackChange(const css::uno::Sequence<sal_uInt32>& rArguments,
                                             bool accept)
{
    SwWrtShell* mrSh = m_pDocShell->GetWrtShell();
    SwDoc* pDoc = mrSh->GetDoc();
    SwUndoId undoId = accept ? SwUndoId::ACCEPT_REDLINE : SwUndoId::REJECT_REDLINE;
    // make batch update a single undo/redo and layout action
    mrSh->StartUndo(undoId, nullptr);
    mrSh->StartAllAction();

    for (sal_uInt32 id : rArguments)
    {
        const SwRedlineTable& rRedlineTable = pDoc->getIDocumentRedlineAccess().GetRedlineTable();
        bool found = false;
        for (SwRedlineTable::size_type i = 0; !found && i < rRedlineTable.size(); ++i)
        {
            if (id == rRedlineTable[i]->GetId())
            {
                found = true;
                if (accept)
                {
                    mrSh->AcceptRedline(i);
                }
                else
                {
                    mrSh->RejectRedline(i);
                }
            }
        }
    }

    mrSh->EndAllAction();
    mrSh->EndUndo(undoId, nullptr);
}

// MACRO-1392: Request layout updates for redlines
void SwXTextDocument::updateRedlines(const css::uno::Sequence<sal_uInt32>& rArguments)
{
    SwWrtShell* mrSh = m_pDocShell->GetWrtShell();
    SwDoc* pDoc = mrSh->GetDoc();

    for (sal_uInt32 id : rArguments)
    {
        const SwRedlineTable& rRedlineTable = pDoc->getIDocumentRedlineAccess().GetRedlineTable();
        bool found = false;
        for (SwRedlineTable::size_type i = 0; !found && i < rRedlineTable.size(); ++i)
        {
            if (id == rRedlineTable[i]->GetId())
            {
                found = true;
                SwRedlineTable::LOKRedlineNotification(RedlineNotification::Modify,
                                                       rRedlineTable[i]);
            }
        }
    }
}

// MACRO-1653/MACRO-1598: Colorize and overlays
void SwXTextDocument::colorize() { colorizer::Colorize(this); }
void SwXTextDocument::cancelColorize() { colorizer::CancelColorize(this); }
void SwXTextDocument::applyOverlays(const char* payload)
{
    colorizer::ApplyOverlays(this, payload);
}
void SwXTextDocument::jumpToOverlay(const char* payload)
{
    colorizer::JumpToOverlay(this, payload);
}
void SwXTextDocument::removeOverlays(const char* payload)
{
    colorizer::ClearOverlays(this, payload);
}

// MACRO-1671: Autorecovery and backup
void SwXTextDocument::setBackupPath(const char* payload)
{
    using boost::property_tree::ptree;
    ptree pt;
    std::istringstream jsonStream(payload);
    read_json(jsonStream, pt);

    GetDocShell()->GetDoc()->getIDocumentTimerAccess().SetBackupPath(
        OUString::fromUtf8(pt.get<std::string>("path")));
}

// MACRO-1165: Set author
void SwXTextDocument::setAuthor(OUString sAuthor)
{
    SAL_WARN("setauthor", "starts");
    SolarMutexGuard aGuard;

    SwView* pView = m_pDocShell->GetView();
    if (!pView)
        return;

    OUString sOrigAuthor = SW_MOD()->GetRedlineAuthor(SW_MOD()->GetRedlineAuthor());
    // Let the actual author name pick up the value from the current
    // view, which would normally happen only during the next view
    // switch.
    pView->SetRedlineAuthor(sAuthor);
    m_pDocShell->SetView(pView);

    if (sAuthor != sOrigAuthor)
    {
        // be lazy, just mark them dirty unlike when initializing
        m_pDocShell->GetDoc()->getIDocumentFieldsAccess().GetUpdateFields().SetFieldsDirty(true);
    }
    SAL_WARN("setauthor", "ends");
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

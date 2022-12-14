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

#include "idlctypes.hxx"
#include "aststack.hxx"
#include "options.hxx"
#include <memory>
#include <string_view>

#ifdef SAL_UNX
#define SEPARATOR '/'
#define PATH_SEPARATOR "/"
#else
#define SEPARATOR '\\'
#define PATH_SEPARATOR "\\"
#endif

class AstInterface;
class AstModule;
class AstType;
class Options;
class ErrorHandler;

class Idlc final
{
public:
    Idlc(Options* pOptions);
    ~Idlc();

    void init();

    bool dumpDeps(std::string_view rDepFile,
                  OString const& rTarget);

    Options* getOptions()
        { return m_pOptions; }
    AstStack* scopes()
        { return m_pScopes.get(); }
    AstModule* getRoot()
        { return m_pRoot.get(); }
    const OString& getFileName() const
        { return m_fileName; }
    void setFileName(const OString& fileName)
        { m_fileName = fileName; addInclude(fileName); }
    const OString& getMainFileName() const
        { return m_mainFileName; }
    void setMainFileName(const OString& mainFileName)
        { m_mainFileName = mainFileName; }
    const OString& getRealFileName() const
        { return m_realFileName; }
    void setRealFileName(const OString& realFileName)
        { m_realFileName = realFileName; }
    const OString& getDocumentation()
        {
            m_bIsDocValid = false;
            return m_documentation;
        }
    void setDocumentation(const OString& documentation)
        {
            m_documentation = documentation;
            m_bIsDocValid = true;
        }
    OUString processDocumentation();
    bool isInMainFile() const
        { return m_bIsInMainfile; }
    void setInMainfile(bool bInMainfile)
        { m_bIsInMainfile = bInMainfile; }
    sal_uInt32 getErrorCount() const
        { return m_errorCount; }
    void incErrorCount()
        { m_errorCount++; }
    sal_uInt32 getWarningCount() const
        { return m_warningCount; }
    void incWarningCount()
        { m_warningCount++; }
    sal_uInt32 getLineNumber() const
        { return m_lineNumber; }
    sal_uInt32 getOffsetStart() const
        { return m_offsetStart; }
    sal_uInt32 getOffsetEnd() const
        { return m_offsetEnd; }
    void setOffset( sal_uInt32 start, sal_uInt32 end)
        { m_offsetStart = start; m_offsetEnd = end; }
    void setLineNumber(sal_uInt32 lineNumber)
        { m_lineNumber = lineNumber; }
    void incLineNumber()
        { m_lineNumber++; }
    ParseState getParseState() const
        { return m_parseState; }
    void setParseState(ParseState parseState)
        { m_parseState = parseState; }

    void addInclude(const OString& inc)
        { m_includes.insert(inc); }

    void setPublished(bool published) { m_published = published; }
    bool isPublished() const { return m_published; }

    void reset();
private:
    Options*            m_pOptions;
    std::unique_ptr<AstStack>  m_pScopes;
    std::unique_ptr<AstModule> m_pRoot;
    OString      m_fileName;
    OString      m_mainFileName;
    OString      m_realFileName;
    OString      m_documentation;
    bool            m_bIsDocValid;
    bool            m_bGenerateDoc;
    bool            m_bIsInMainfile;
    bool                m_published;
    sal_uInt32          m_errorCount;
    sal_uInt32          m_warningCount;
    sal_uInt32          m_lineNumber;
    sal_uInt32          m_offsetStart;
    sal_uInt32          m_offsetEnd;
    ParseState          m_parseState;
    std::set< OString >           m_includes;
};


typedef ::std::pair< OString, OString > sPair_t;
sal_Int32 compileFile(const OString * pathname);
    // a null pathname means stdin
sal_Int32 produceFile(const OString& filenameBase,
        sPair_t const*const pDepFile);
    // filenameBase is filename without ".idl"
void removeIfExists(std::string_view pathname);

bool copyFile(const OString* source, const OString& target);
    // a null source means stdin

bool isFileUrl(std::string_view fileName);
OString convertToAbsoluteSystemPath(const OString& fileName);
OString convertToFileUrl(const OString& fileName);

Idlc* idlc();
Idlc* setIdlc(Options* pOptions);

AstDeclaration const * resolveTypedefs(AstDeclaration const * type);

AstDeclaration const * deconstructAndResolveTypedefs(
    AstDeclaration const * type, sal_Int32 * rank);

AstInterface const * resolveInterfaceTypedefs(AstType const * type);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

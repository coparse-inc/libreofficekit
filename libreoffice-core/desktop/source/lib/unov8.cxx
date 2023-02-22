/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <map>
#include <com/sun/star/uno/Any.hxx>
#include <uno/sequence2.h>
#include <lib/unov8.hxx>
#include <unov8.hxx>

namespace desktop
{

namespace
{
static rtl_uString* uStringFromUtf8(const char* str, int length)
{
    if (str == nullptr)
        return nullptr;

    rtl_uString* result = nullptr;
    rtl_uString_new(&result);
    return rtl_convertStringToUString(&result, str, length, RTL_TEXTENCODING_UTF8,
                                      RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_ERROR
                                          | RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_ERROR
                                          | RTL_TEXTTOUNICODE_FLAGS_INVALID_ERROR)
               ? result
               : nullptr;
}

static rtl_String* uStringToUtf8(rtl_uString* str)
{
    if (str == nullptr)
        return nullptr;

    rtl_String* result = nullptr;
    rtl_string_new(&result);
    return rtl_convertUStringToString(&result, str->buffer, str->length, RTL_TEXTENCODING_UTF8,
                                      RTL_UNICODETOTEXT_FLAGS_UNDEFINED_ERROR
                                          | RTL_UNICODETOTEXT_FLAGS_INVALID_ERROR)
               ? result
               : nullptr;
}

static uno_Sequence* sequence_construct(typelib_TypeDescriptionReference* type, void* elements,
                                        int len)
{
    uno_Sequence* sequence = nullptr;
    if (elements == nullptr && len != 0)
        return nullptr;
    return uno_type_sequence_construct(&sequence, type, elements, len, css::uno::cpp_acquire)
               ? sequence
               : nullptr;
}

static uno_Any* any_construct(void* value, typelib_TypeDescriptionReference* type)
{
    auto* result = new css::uno::Any(value, type);
    return (uno_Any*)result;
}

static typelib_TypeDescriptionReference* sequenceType(typelib_TypeDescriptionReference* type)
{
    static std::map<typelib_TypeDescriptionReference*, typelib_TypeDescriptionReference*>
        seqTypeMap;

    auto result = seqTypeMap.find(type);
    if (result == seqTypeMap.end())
    {
        typelib_TypeDescriptionReference* p = nullptr;
        ::typelib_static_sequence_type_init(&p, type);
        if (p == nullptr)
        {
            return nullptr;
        }
        seqTypeMap.emplace(type, p);
        return p;
    }
    return result->second;
}

} // namespace

void unov8_init(UnoV8& uno_v8)
{
    uno_v8.rtl = {
        .uStringFromUtf8 = uStringFromUtf8,
        .uStringToUtf8 = uStringToUtf8,
        .uString_acquire = rtl_uString_acquire,
        .uString_release = rtl_uString_release,
        .string_acquire = rtl_string_acquire,
        .string_release = rtl_string_release,
    };
    uno_v8.interface = {
        .as = ::unov8::as,
        .queryInterface = css::uno::cpp_queryInterface,
        .acquire = css::uno::cpp_acquire,
        .release = css::uno::cpp_release,
    };
    uno_v8.type = {
        .sequenceType = sequenceType,
        .structType = ::unov8::structType,
        .enumType = ::unov8::enumType,
        .interfaceType = ::unov8::interfaceType,
        .getByTypeClass = typelib_static_type_getByTypeClass,
    };
    uno_v8.sequence = { .construct = sequence_construct };
    uno_v8.any = { .construct = any_construct };
}

} // namespace desktop

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef INCLUDED_LIBREOFFICEKIT_UNOV8_H
#define INCLUDED_LIBREOFFICEKIT_UNOV8_H

#include "uno/any2.h"
#include "uno/sequence2.h"
#include <typelib/typedescription.h>
#include <sal/types.h>
#include <rtl/string.h>
#include <rtl/ustring.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct _UnoV8 UnoV8;

    struct _UnoV8Rtl
    {
        /** wrapper around rtl_convertStringToUString
@returns nullptr for failure to convert, valid pointer for success */
        rtl_uString* (*uStringFromUtf8)(const char* str, int length);

        /** wrapper around rtl_convertUStringToString
@returns nullptr for failure to convert, valid pointer for success */
        rtl_String* (*uStringToUtf8)(rtl_uString* str);

        // #see rtl/string.h and rtl/ustring.h
        void (*uString_acquire)(rtl_uString* str);
        void (*uString_release)(rtl_uString* str);
        void (*string_acquire)(rtl_String* str);
        void (*string_release)(rtl_String* str);
    };

    struct _UnoV8Type
    {
        /** @returns the sequence type corresponding to the provided type */
        typelib_TypeDescriptionReference* (*sequenceType)(typelib_TypeDescriptionReference* type);
        /** @returns the struct type corresponding to the provided type ID */
        typelib_TypeDescriptionReference* (*structType)(unsigned int type_id);
        /** @returns the struct type corresponding to the provided type ID */
        typelib_TypeDescriptionReference* (*enumType)(unsigned int type_id);
        /** @returns the sequence type corresponding to the provided type name */
        typelib_TypeDescriptionReference* (*interfaceType)(const char* str, int str_len);

        // @see typelib/typedescription.h
        typelib_TypeDescriptionReference** (*getByTypeClass)(typelib_TypeClass eTypeClass);
    };

    struct _UnoV8Interface
    {
        /** give an interface attempts to cast to the type corresponding to type_name
         * @returns NULL if the interface is not cast successfully, otherwise returns the aligned pointer to the interface as the provided type
         **/
        void* (*as)(void* interface, const char* type_name, int type_name_len);

        // @see uno/data.h or com/star/sun/uno/XInterface.hdl
        void* (*queryInterface)(void* interface, typelib_TypeDescriptionReference* type);
        void (*acquire)(void* interface);
        void (*release)(void* interface);
    };

    struct _UnoV8Sequence
    {
        /** provided a type, the pointer the the elements of an array, and the length of the array, constructs a sequence of that type
         * @returns NULL if the sequence construction failed, otherwise returns a pointer to the new uno_Sequence
         **/
        uno_Sequence* (*construct)(typelib_TypeDescriptionReference* type, void* elements, int len);
    };

    struct _UnoV8Any
    {
        /** provided a type, and a pointer to a value, constructs an uno_Any of that type
         * @returns NULL if the Any construction failed, otherwise returns a pointer to the new uno_Any
         **/
        uno_Any* (*construct)(void* value, typelib_TypeDescriptionReference* type);
    };

    struct _UnoV8
    {
        struct _UnoV8Rtl rtl;
        struct _UnoV8Type type;
        struct _UnoV8Interface interface;
        struct _UnoV8Sequence sequence;
        struct _UnoV8Any any;
    };

#ifdef __cplusplus
}
#endif

#endif // INCLUDED_LIBREOFFICEKIT_UNOV8_H

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
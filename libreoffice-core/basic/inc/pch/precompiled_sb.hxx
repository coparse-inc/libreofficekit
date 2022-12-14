/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 This file has been autogenerated by update_pch.sh. It is possible to edit it
 manually (such as when an include file has been moved/renamed/removed). All such
 manual changes will be rewritten by the next run of update_pch.sh (which presumably
 also fixes all possible problems, so it's usually better to use it).

 Generated on 2021-09-12 11:49:52 using:
 ./bin/update_pch basic sb --cutoff=2 --exclude:system --exclude:module --include:local

 If after updating build fails, use the following command to locate conflicting headers:
 ./bin/update_pch_bisect ./basic/inc/pch/precompiled_sb.hxx "make basic.build" --find-conflicts
*/

#include <sal/config.h>
#if PCH_LEVEL >= 1
#include <chrono>
#include <cstddef>
#include <math.h>
#include <memory>
#include <ostream>
#include <stdio.h>
#include <stdlib.h>
#include <string_view>
#include <type_traits>
#include <vector>
#endif // PCH_LEVEL >= 1
#if PCH_LEVEL >= 2
#include <osl/endian.h>
#include <osl/file.hxx>
#include <osl/process.h>
#include <osl/thread.h>
#include <osl/time.h>
#include <rtl/character.hxx>
#include <rtl/math.h>
#include <rtl/math.hxx>
#include <rtl/string.hxx>
#include <rtl/textenc.h>
#include <rtl/ustrbuf.hxx>
#include <rtl/ustring.h>
#include <rtl/ustring.hxx>
#include <sal/log.hxx>
#include <sal/saldllapi.h>
#include <sal/types.h>
#include <vcl/bitmap.hxx>
#include <vcl/cairo.hxx>
#include <vcl/devicecoordinate.hxx>
#include <vcl/dllapi.h>
#include <vcl/errcode.hxx>
#include <vcl/font.hxx>
#include <vcl/graph.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/metaactiontypes.hxx>
#include <vcl/outdev.hxx>
#include <vcl/region.hxx>
#include <vcl/rendercontext/AddFontSubstituteFlags.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/DrawGridFlags.hxx>
#include <vcl/rendercontext/DrawImageFlags.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>
#include <vcl/rendercontext/DrawTextFlags.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/rendercontext/ImplMapRes.hxx>
#include <vcl/rendercontext/InvertFlags.hxx>
#include <vcl/rendercontext/RasterOp.hxx>
#include <vcl/rendercontext/SalLayoutFlags.hxx>
#include <vcl/rendercontext/State.hxx>
#include <vcl/rendercontext/SystemTextColorFlags.hxx>
#include <vcl/salnativewidgets.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/vclreferencebase.hxx>
#include <vcl/wall.hxx>
#include <vcl/weld.hxx>
#endif // PCH_LEVEL >= 2
#if PCH_LEVEL >= 3
#include <basegfx/color/bcolor.hxx>
#include <basegfx/numeric/ftools.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/vector/b2enums.hxx>
#include <com/sun/star/awt/DeviceInfo.hpp>
#include <com/sun/star/drawing/LineCap.hpp>
#include <com/sun/star/lang/Locale.hpp>
#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/uno/Sequence.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/string.hxx>
#include <cppuhelper/weakref.hxx>
#include <i18nlangtag/lang.h>
#include <o3tl/char16_t2wchar_t.hxx>
#include <o3tl/cow_wrapper.hxx>
#include <o3tl/float_int_conversion.hxx>
#include <o3tl/safeint.hxx>
#include <svl/SfxBroadcaster.hxx>
#include <svl/numformat.hxx>
#include <svl/zforlist.hxx>
#include <tools/color.hxx>
#include <tools/debug.hxx>
#include <tools/gen.hxx>
#include <tools/link.hxx>
#include <tools/mapunit.hxx>
#include <tools/poly.hxx>
#include <tools/ref.hxx>
#include <tools/solar.h>
#include <tools/stream.hxx>
#include <tools/toolsdllapi.h>
#include <tools/urlobj.hxx>
#include <tools/wintypes.hxx>
#include <unotools/charclass.hxx>
#include <unotools/fontdefs.hxx>
#include <unotools/syslocale.hxx>
#include <unotools/unotoolsdllapi.h>
#endif // PCH_LEVEL >= 3
#if PCH_LEVEL >= 4
#include <basic/basicdllapi.h>
#include <basic/sbdef.hxx>
#include <basic/sberrors.hxx>
#include <basic/sbmod.hxx>
#include <basic/sbstar.hxx>
#include <basic/sbuno.hxx>
#include <basic/sbx.hxx>
#include <basic/sbxmeth.hxx>
#include <basic/sbxobj.hxx>
#include <basic/sbxvar.hxx>
#include <date.hxx>
#include <iosys.hxx>
#include <rtlproto.hxx>
#include <runtime.hxx>
#include <sbintern.hxx>
#include <sbobjmod.hxx>
#include <sbunoobj.hxx>
#include <sbxbase.hxx>
#include <sbxfac.hxx>
#include <sbxform.hxx>
#include <sbxprop.hxx>
#endif // PCH_LEVEL >= 4

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

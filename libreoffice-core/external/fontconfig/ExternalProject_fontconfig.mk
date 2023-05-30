# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_ExternalProject_ExternalProject,fontconfig))

$(eval $(call gb_ExternalProject_use_externals,fontconfig,\
	$(if $(filter EMSCRIPTEN,$(OS)),libxml2,expat) \
	freetype \
))

$(eval $(call gb_ExternalProject_register_targets,fontconfig,\
	build \
))

# Can't have this inside the $(call gb_ExternalProject_run as it contains commas
fontconfig_add_fonts_linux=/usr/share/X11/fonts/Type1,/usr/share/X11/fonts/TTF,/usr/local/share/fonts
fontconfig_add_fonts_mac=/System/Library/Fonts,/Library/Fonts,~/Library/Fonts,/System/Library/AssetsV2/com_apple_MobileAsset_Font7

$(call gb_ExternalProject_get_state_target,fontconfig,build) :
	$(call gb_Trace_StartRange,fontconfig,EXTERNAL)
	$(call gb_ExternalProject_run,build,\
		CFLAGS="$(CFLAGS) \
			$(call gb_ExternalProject_get_build_flags,fontconfig) \
			$(if $(filter WNT,$(OS)), -nologo -MD) \
			$(if $(debug),-g) \
			$(gb_VISIBILITY_FLAGS) \
			$(if $(filter EMSCRIPTEN,$(OS)),-pthread)" \
			$(if $(filter ANDROID,$(OS)),LIBS="-lm") \
		$(if $(filter EMSCRIPTEN,$(OS)),LIBXML2_CFLAGS="$(LIBXML_CFLAGS)" LIBXML2_LIBS="$(LIBXML_LIBS)") \
		$(if $(filter WNT,$(OS)),\
			FREETYPE_CFLAGS="-I$(call gb_UnpackedTarball_get_dir,freetype)/include" \
			FREETYPE_LIBS="-L$(call gb_UnpackedTarball_get_dir,freetype)/instdir/lib -lfreetype" \
		) \
		$(gb_RUN_CONFIGURE) ./configure \
			--with-pic \
			--disable-shared \
			--disable-silent-rules \
			$(if $(filter ANDROID,$(OS)),--with-arch=arm) \
			--with-expat-includes=$(call gb_UnpackedTarball_get_dir,expat)/lib \
			--with-expat-lib=$(gb_StaticLibrary_WORKDIR) \
			$(gb_CONFIGURE_PLATFORMS) \
			$(if $(filter ANDROID,$(OS)), \
				--disable-shared \
			) \
			$(if $(filter EMSCRIPTEN,$(OS)), \
				--disable-shared \
			    --with-baseconfigdir=/instdir/share/fontconfig \
			    --with-cache-dir=/instdir/share/fontconfig/cache \
			    --with-add-fonts=/instdir/share/fonts \
			    --enable-libxml2 \
			    ac_cv_func_fstatfs=no ac_cv_func_fstatvfs=no \
			) \
			$(if $(filter LINUX,$(OS)), \
				--with-add-fonts=$(fontconfig_add_fonts_linux) \
				--with-cache-dir=/usr/lib/fontconfig/cache \
			) \
			$(if $(filter MACOSX,$(OS)), \
				--with-add-fonts=$(fontconfig_add_fonts_mac) \
				--without-libintl-prefix \
			) \
		&& $(MAKE) -C src && $(MAKE) fonts.conf \
	)
	$(call gb_Trace_EndRange,fontconfig,EXTERNAL)

# vim: set noet sw=4 ts=4:

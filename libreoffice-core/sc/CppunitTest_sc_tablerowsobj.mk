# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#*************************************************************************
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#*************************************************************************

$(eval $(call gb_CppunitTest_CppunitTest,sc_tablerowsobj))

$(eval $(call gb_CppunitTest_use_external,sc_tablerowsobj,boost_headers))

$(eval $(call gb_CppunitTest_use_common_precompiled_header,sc_tablerowsobj))

$(eval $(call gb_CppunitTest_add_exception_objects,sc_tablerowsobj, \
    sc/qa/extras/sctablerowsobj \
))

$(eval $(call gb_CppunitTest_use_libraries,sc_tablerowsobj, \
	cppu \
	sal \
	salhelper \
	sc \
	subsequenttest \
	test \
	unotest \
))

$(eval $(call gb_CppunitTest_set_include,sc_tablerowsobj,\
	-I$(SRCDIR)/sc/inc \
	$$(INCLUDE) \
))

$(eval $(call gb_CppunitTest_use_sdk_api,sc_tablerowsobj))

$(eval $(call gb_CppunitTest_use_ure,sc_tablerowsobj))
$(eval $(call gb_CppunitTest_use_vcl,sc_tablerowsobj))

$(eval $(call gb_CppunitTest_use_components,sc_tablerowsobj,\
    $(sc_unoapi_common_components) \
))

$(eval $(call gb_CppunitTest_use_configuration,sc_tablerowsobj))

# vim: set noet sw=4 ts=4:

# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Module_Module,extensions))

$(eval $(call gb_Module_add_l10n_targets,extensions,\
	AllLangMoTarget_pcr \
))

ifeq (,$(DISABLE_DYNLOADING))
$(eval $(call gb_Module_add_targets,extensions,\
	Library_abp \
	Library_scn \
	$(if $(filter WNT,$(OS)), \
		Library_WinUserInfoBe \
		$(if $(filter TRUE,$(BUILD_X86)),Executable_twain32shim) \
	) \
	UIConfig_sabpilot \
	UIConfig_scanner \
))
endif

ifneq ($(filter-out iOS,$(OS)),)
$(eval $(call gb_Module_add_targets,extensions,\
	Library_log \
))
endif

ifeq ($(ENABLE_LDAP),TRUE)
$(eval $(call gb_Module_add_targets,extensions,\
	Library_ldapbe2 \
))
endif

$(eval $(call gb_Module_add_targets,extensions,\
	Library_bib \
))

$(eval $(call gb_Module_add_check_targets,extensions,\
    CppunitTest_extensions_bibliography \
))

ifneq (,$(filter DBCONNECTIVITY,$(BUILD_TYPE)))
$(eval $(call gb_Module_add_targets,extensions,\
	Library_dbp \
	Library_pcr \
	UIConfig_sbibliography \
	UIConfig_spropctrlr \
))
endif

ifneq (,$(filter DESKTOP,$(BUILD_TYPE)))
ifeq (,$(ENABLE_WASM_STRIP_BASIC_DRAW_MATH_IMPRESS))
$(eval $(call gb_Module_add_targets,extensions,\
	Library_updatefeed \
))

ifeq ($(ENABLE_ONLINE_UPDATE),TRUE)
$(eval $(call gb_Module_add_targets,extensions,\
	Configuration_updchk \
	Library_updatecheckui \
	Library_updchk \
))

$(eval $(call gb_Module_add_check_targets,extensions,\
    CppunitTest_extensions_test_update \
))
endif
endif # !ENABLE_WASM_STRIP_BASIC_DRAW_MATH_IMPRESS
endif

# ifeq ($(OS),MACOSX)
# $(eval $(call gb_Module_add_targets,extensions,\
# 	Library_OOoSpotlightImporter \
# 	Package_mdibundle \
# 	Package_OOoSpotlightImporter \
# ))
# endif # OS=MACOSX

$(eval $(call gb_Module_add_subsequentcheck_targets,extensions,\
    JunitTest_extensions_unoapi \
))

# vim:set shiftwidth=4 softtabstop=4 noexpandtab:

# Makefile for the EPICS V4 pvAccess module

TOP = .
include $(TOP)/configure/CONFIG
DIRS := $(DIRS) $(filter-out $(DIRS), configure)
DIRS := $(DIRS) $(filter-out $(DIRS), src)
DIRS := $(DIRS) $(filter-out $(DIRS), pvaSrc)
DIRS := $(DIRS) $(filter-out $(DIRS), pvtoolsSrc)
DIRS := $(DIRS) $(filter-out $(DIRS), testApp)
DIRS := $(DIRS) $(filter-out $(DIRS), pvaExample)
DIRS := $(DIRS) $(filter-out $(DIRS), pvaTest)

EMBEDDED_TOPS := $(EMBEDDED_TOPS) $(filter-out $(EMBEDDED_TOPS), pvaExample)
EMBEDDED_TOPS := $(EMBEDDED_TOPS) $(filter-out $(EMBEDDED_TOPS), pvaTest)

define DIR_template
 $(1)_DEPEND_DIRS = configure
endef
$(foreach dir, $(filter-out configure,$(DIRS)),$(eval $(call DIR_template,$(dir))))

define EMB_template
 $(1)_DEPEND_DIRS = src pvaSrc
endef
$(foreach dir, $(EMBEDDED_TOPS),$(eval $(call EMB_template,$(dir))))

include $(TOP)/configure/RULES_TOP

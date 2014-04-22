# Makefile for the EPICS V4 pvAccess module

TOP = .
include $(TOP)/configure/CONFIG

DIRS := configure

DIRS += src
src_DEPEND_DIRS = configure

DIRS += pvtoolsSrc
pvtoolsSrc_DEPEND_DIRS = src

DIRS += testApp
testApp_DEPEND_DIRS = src

include $(TOP)/configure/RULES_TOP

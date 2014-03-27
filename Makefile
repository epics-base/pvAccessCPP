# Makefile for the EPICS V4 pvAccess module

TOP = .
include $(TOP)/configure/CONFIG

DIRS := configure

DIRS += pvAccessApp
pvAccessApp_DEPEND_DIRS = configure

DIRS += testApp
testApp_DEPEND_DIRS = pvAccessApp

include $(TOP)/configure/RULES_TOP

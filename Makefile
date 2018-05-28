# Makefile for the EPICS V4 pvAccess module

TOP = .
include $(TOP)/configure/CONFIG

DIRS := configure

DIRS += src
src_DEPEND_DIRS = configure

DIRS += src/ca
src/ca_DEPEND_DIRS = src

DIRS += src/ioc
src/ioc_DEPEND_DIRS = src

DIRS += pvtoolsSrc
pvtoolsSrc_DEPEND_DIRS = src src/ca

DIRS += testApp
testApp_DEPEND_DIRS = src


DIRS += testCa
testCa_DEPEND_DIRS = src src/ca


DIRS += examples
examples_DEPEND_DIRS += src src/ca

include $(TOP)/configure/RULES_TOP

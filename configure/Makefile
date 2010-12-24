TOP=..

include $(TOP)/configure/CONFIG

# CHECK_RELEASE controls the consistency checking of the support
#   applications pointed to by the RELEASE* files.
# Normally CHECK_RELEASE should be set to YES.
# Set CHECK_RELEASE to NO to disable checking completely.
# Set CHECK_RELEASE to WARN to perform consistency checking but
#   continue building anyway if conflicts are found.
CHECK_RELEASE = YES

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

include $(TOP)/configure/RULES

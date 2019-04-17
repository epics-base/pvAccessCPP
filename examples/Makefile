TOP=..

include $(TOP)/configure/CONFIG

PROD_LIBS += pvAccessCA ca pvAccess pvData Com
PROD_SYS_LIBS_WIN32 += netapi32 ws2_32

TESTPROD_HOST += getme
getme_SRCS = getme.cpp

TESTPROD_HOST += putme
putme_SRCS = putme.cpp

TESTPROD_HOST += monitorme
monitorme_SRCS = monitorme.cpp

TESTPROD_HOST += spamme
spamme_SRCS = spamme.cpp

TESTPROD_HOST += mailbox
mailbox_SRCS += mailbox.cpp

TESTPROD_HOST += epicschat
epicschat_SRCS += epicschat.cpp

TESTPROD_HOST += lazycounter
lazycounter_SRCS += lazycounter.cpp

TESTPROD_HOST += miniget
miniget_SRCS = miniget.cpp

TESTPROD_HOST += miniput
miniput_SRCS = miniput.cpp

TESTPROD_HOST += minimonitor
minimonitor_SRCS = minimonitor.cpp

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


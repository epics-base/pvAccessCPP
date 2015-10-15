pvAccessCPP
==========

pvAccess is a computer communications protocol for control systems, and is a central 
component of the EPICS software toolkit. pvAccessCPP is the name of the software 
module which contains the C++ implementation of pvAccess.


Further Info
------------

Consult the documents in the documentation directory, in particular

* pvAccessCPP.html
* RELEASE_NOTES.md

Also see the [EPICS Version 4 website](http://epics-pvdata.sourceforge.net)

Prerequisites
-------------

The pvAccessCPP  requires recent versions of the following software:

1. EPICS Base (v3.14.12.3 or later)
2. EPICS4 pvCommonCPP (4.1.1 or later)
3. EPICS4 pvDataCPP (5.0.2 or later)


Building
--------

Building uses the make utility and the EPICS base build system.

The build system needs the location of the prerequisites, e.g. by placing the
lines of the form

    PVCOMMON = /home/install/epicsV4/pvCommonCPP
    PVDATA = /home/install/epicsV4/pvDataCPP
    EPICS_BASE = /home/install/epics/base

pointing to the locations in a file called RELEASE.local
in the configure directory or the parent directory of pvAccessCPP.

With this in place, to build type make

    make

To perform a clean build type

    make clean uninstall

To run the unit tests type

    make runtests

For more information on the EPICS build system consult the
[Application Development guide](http://www.aps.anl.gov/epics/base/R3-14/12-docs/AppDevGuide.pdf).


Example Usage
-------------
This section describes how you can test and demo pvAccess.

A test server is shipped with pvAccessCPP. See the file pvAccessCPP/DEMO for
examples of usage. To run the server, write a setup script like that above, and
then use it prior to executing "runTestServer":

    $ ./runTestServer 
    Starting pvAccess C++ test server...
    VERSION : pvAccess Server v4.1.1
    PROVIDER_NAMES : local
    BEACON_ADDR_LIST : 
    AUTO_BEACON_ADDR_LIST : 1
    BEACON_PERIOD : 15
    BROADCAST_PORT : 5076
    SERVER_PORT : 5075
    RCV_BUFFER_SIZE : 16384
    IGNORE_ADDR_LIST: 
    STATE : INITIALIZED

Then, another window, you can go through the demos in pvAccessCPP/DEMO. For
example:

    $ ./bin/$EPICS_HOST_ARCH/pvget testValue
    testValue                     0

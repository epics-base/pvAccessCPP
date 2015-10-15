README OF pvAccessCPP

pvAccess is a computer communications protocol for control systems, and is a central 
component of the EPICS software toolkit. PvAccessCPP is the name of the software 
module which contains the C++ implementation of pvAccess.

---------------------------------------------------------------------------
Auth: Matej Sekoranja, matej.sekoranja@cosylab.com, Oct-2011.
Mod:  Greg White, greg@slac.stanford.edu, 28-Aug-2013
      Added USAGE and header and stuff.
===========================================================================

pvAccessCPP is sourced in the "EPICS Version 4 project in Sourceforge [1].  
pvAccessJava, the Java implementation of PVAccess [3].

BUILD
=====
This section describes how to compile, link and install pvAccessCPP.

[To be added]

USAGE
=====
This section describes how to set up the runtime environment of pvAccessCPP.

For simplicity, the following describes the setup of a pvAccess endpoint,
regardless of whether the endpoint is server or client side. In practice, parts
of each step may be omitted depending on whether the endpoint is a server or
client.
 
1. Define the following environment variables:

      EPICS_BASE - MUST be assigned to the top level directory of a local installation of 
                   EPICS base, of version 3.14; per the RELEASE.local file
                   used to build this pvAccessCPP. 
      PVCOMMON   - MUST be assigned to the top level directory of a local installation of
                   pvCommonCPP [5], per the RELEASE.local file
                   used to build this pvAccessCPP.
      PVDATA     - MUST be assigned to the top level directory of a local installation of
                   pvDataCPP [4], per the RELEASE.local file
                   used to build this pvAccessCPP. 

2. Add the pvAccessCPP binaries to your PATH.

   Regarding EPICS_HOST_ARCH: in general pvAccessCPP endpoints will attempt to  
   establish their runtime architecture at runtime using
   $EPICS_BASE/startup/EpicsHostArch, but bear in mind that script is not
   completely reliable. It's a good idea to set EPICS_HOST_ARCH explicitly in
   your setup.

   
Example Usage
-------------
For example, the following source bash script can be used to set up both client
and server processes for running and usign the pvAccess test server
(testServer). testServer can be found in the root directory of pvAccessCPP.

$ cat testServer_setup.bash 
export EPICS_BASE=${HOME}/Development/epicsV3/base-3.14.12.2
export PVCOMMON=${HOME}/Development/epicsV4/4-3test/CPP-4-3-0/pvCommonCPP
export PVDATA=${HOME}/Development/epicsV4/4-3test/CPP-4-3-0/pvDataCPP
export EPICS_HOST_ARCH=darwin-x86
export PATH=${HOME}/Development/epicsv4/4-3test/CPP-4-3-0/pvAccessCPP/bin/${EPICS_HOST_ARCH}:${PATH}

TESTING
=======
This section describes how you can test and demo pvAccess.

A test server is shipped with pvAccessCPP. See the file pvAccessCPP/DEMO for
examples of usage. To run the server, write a setup script like that above, and
then use it prior to executing "runTestServer":

$ source testServer_setup.bash
$ ./runTestServer 
Starting pvAccess C++ test server...
VERSION : pvAccess Server v3.0.1
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

$ pvget testValue
testValue                     0




 



REFERENCES
==========
[1] http://epics-pvdata.sourceforge.net
[2] https://github.com/epics-base/pvAccessCPP
[3] https://github.com/epics-base/pvAccessJava
[4] https://github.com/epics-base/pvDataCPP
[5] https://github.com/epics-base/pvCommonCPP


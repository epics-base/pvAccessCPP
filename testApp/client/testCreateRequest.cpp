/* testCreateRequest.cpp */
/* Author:  Matej Sekoranja Date: 2010.12.27 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <pv/pvAccess.h>
#include <iostream>

#include <epicsAssert.h>
#include <epicsExit.h>

using namespace epics::pvData;
using namespace epics::pvAccess;


void testCreateRequest() {
    printf("testCreateRequest... \n");
    CreateRequest::shared_pointer  createRequest = CreateRequest::create();

    String out;
    String request = "";
    std::cout << std::endl << String("request") <<std::endl << request <<std::endl;
    PVStructurePtr pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

request = "record[process=true]field(alarm,timeStamp)putField(synput:a,synput:b,stnput:c)";
        std::cout << std::endl << String("request") <<std::endl << request <<std::endl;
pvRequest = createRequest->createRequest(request);
assert(pvRequest.get());
out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;


    request = "alarm,timeStamp,power.value";
        std::cout << std::endl << String("request") <<std::endl << request <<std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "record[process=true]field(alarm,timeStamp,power.value)";
        std::cout << std::endl << String("request") <<std::endl << request <<std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "record[process=true]field(alarm,timeStamp[algorithm=onChange,causeMonitor=false],power{value,alarm})";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "record[process=true,xxx=yyy]field(alarm,timeStamp[shareData=true],power.value)";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = String("record[process=true,xxx=yyy]")
        + "putField(power.value)"
    	+ "getField(alarm,timeStamp,power{value,alarm},"
    	+ "current{value,alarm},voltage{value,alarm})";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = String("field(alarm,timeStamp,supply{")
    	+ "0{voltage.value,current.value,power.value},"
        + "1{voltage.value,current.value,power.value}"
        + "})";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = String("record[process=true,xxx=yyy]")
    	+ "putField(power.value)"
    	+ "getField(alarm,timeStamp,power{value,alarm},"
    	+ "current{value,alarm},voltage{value,alarm},"
    	+ "ps0{alarm,timeStamp,power{value,alarm},current{value,alarm},voltage{value,alarm}},"
        + "ps1{alarm,timeStamp,power{value,alarm},current{value,alarm},voltage{value,alarm}}"
        + ")";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "a{b{c{d}}}";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "record[process=true,xxx=yyy]field(alarm,timeStamp[shareData=true],power.value";
        std::cout << String("request") <<std::endl << request <<std::endl;
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get()==NULL);
    std::cout << "reason " << createRequest->getMessage() << std::endl;
    request = String("record[process=true,xxx=yyy]")
        + "putField(power.value)"
        + "getField(alarm,timeStamp,power{value,alarm},"
        + "current{value,alarm},voltage{value,alarm},"
        + "ps0{alarm,timeStamp,power{value,alarm},current{value,alarm},voltage{value,alarm}},"
        + "ps1{alarm,timeStamp,power{value,alarm},current{value,alarm},voltage{value,alarm}"
        + ")";
        std::cout << String("request") <<std::endl << request <<std::endl;
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get()==NULL);
    std::cout << "reason " << createRequest->getMessage() << std::endl;
    request = "record[process=true,power.value";
        std::cout << String("request") <<std::endl << request <<std::endl;
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = createRequest->createRequest(request);
    assert(pvRequest.get()==NULL);
    std::cout << "reason " << createRequest->getMessage() << std::endl;
}

int main()
{
    testCreateRequest();

    //std::cout << "-----------------------------------------------------------------------" << std::endl;
    //epicsExitCallAtExits();
    return 0;
}



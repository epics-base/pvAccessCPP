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

class RequesterImpl : public Requester,
     public std::tr1::enable_shared_from_this<RequesterImpl>
{
public:

    virtual String getRequesterName()
    {
        return "RequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }
};

void testCreateRequest() {
    printf("testCreateRequest... \n");
    Requester::shared_pointer requester(new RequesterImpl());
    CreateRequest::shared_pointer  createRequest = getCreateRequest();

    String out;
    String request = "";
        std::cout << std::endl << String("request") <<std::endl << request <<std::endl;
        epics::pvData::PVStructure::shared_pointer pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

request = "record[process=true]field(alarm,timeStamp)putField(synput:a,synput:b,stnput:c)";
        std::cout << std::endl << String("request") <<std::endl << request <<std::endl;
pvRequest = getCreateRequest()->createRequest(request,requester);
assert(pvRequest.get());
out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;


    request = "alarm,timeStamp,power.value";
        std::cout << std::endl << String("request") <<std::endl << request <<std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "record[process=true]field(alarm,timeStamp,power.value)";
        std::cout << std::endl << String("request") <<std::endl << request <<std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "record[process=true]field(alarm,timeStamp[algorithm=onChange,causeMonitor=false],power{value,alarm})";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "record[process=true,xxx=yyy]field(alarm,timeStamp[shareData=true],power.value)";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = String("record[process=true,xxx=yyy]")
        + "putField(power.value)"
    	+ "getField(alarm,timeStamp,power{value,alarm},"
    	+ "current{value,alarm},voltage{value,alarm})";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = String("field(alarm,timeStamp,supply{")
    	+ "0{voltage.value,current.value,power.value},"
        + "1{voltage.value,current.value,power.value}"
        + "})";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
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
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "a{b{c{d}}}";
        std::cout << String("request") <<std::endl << request <<std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get());
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;

    request = "record[process=true,xxx=yyy]field(alarm,timeStamp[shareData=true],power.value";
        std::cout << String("request") <<std::endl << request <<std::endl;
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get()==NULL);
    request = String("record[process=true,xxx=yyy]")
        + "putField(power.value)"
        + "getField(alarm,timeStamp,power{value,alarm},"
        + "current{value,alarm},voltage{value,alarm},"
        + "ps0{alarm,timeStamp,power{value,alarm},current{value,alarm},voltage{value,alarm}},"
        + "ps1{alarm,timeStamp,power{value,alarm},current{value,alarm},voltage{value,alarm}"
        + ")";
        std::cout << String("request") <<std::endl << request <<std::endl;
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get()==NULL);
    request = "record[process=true,power.value";
        std::cout << String("request") <<std::endl << request <<std::endl;
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,requester);
    assert(pvRequest.get()==NULL);
}

int main()
{
    testCreateRequest();

    //std::cout << "-----------------------------------------------------------------------" << std::endl;
    //epicsExitCallAtExits();
    return 0;
}



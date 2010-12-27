/* testCreateRequest.cpp */
/* Author:  Matej Sekoranja Date: 2010.12.27 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <pvAccess.h>
#include <showConstructDestruct.h>
#include <iostream>

#include <epicsAssert.h>

using namespace epics::pvData;
using namespace epics::pvAccess;


class RequesterImpl : public Requester {
    public:
    
    virtual String getRequesterName()
    {
        return "RequesterImpl";
    };
    
    virtual void message(String message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }
};
    

void testCreateRequest() {
    printf("testCreateRequest... ");

    RequesterImpl requester;
    String out;
	String request = "";
    PVStructure* pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "alarm,timeStamp,power.value";
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "record[process=true]field(alarm,timeStamp,power.value)";
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;
    
    request = "record[process=true]field(alarm,timeStamp[algorithm=onChange,causeMonitor=false],power{power.value,power.alarm})";
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "record[process=true,xxx=yyy]field(alarm,timeStamp[shareData=true],power.value)";
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = String("record[process=true,xxx=yyy]")
    	+ "putField(power.value)"
    	+ "getField(alarm,timeStamp,power{power.value,power.alarm},"
    	+ "current{current.value,current.alarm},voltage{voltage.value,voltage.alarm})";
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = String("record[process=true,xxx=yyy]")
    	+ "putField(power.value)"
    	+ "getField(alarm,timeStamp,power{power.value,power.alarm},"
    	+ "current{current.value,current.alarm},voltage{voltage.value,voltage.alarm},"
    	+ "ps0{"
    	+ "ps0.alarm,ps0.timeStamp,power{ps0.power.value,ps0.power.alarm},"
    	+ "current{ps0.current.value,ps0.current.alarm},voltage{ps0.voltage.value,ps0.voltage.alarm}},"
    	+ "ps1{"
    	+ "ps1.alarm,ps1.timeStamp,power{ps1.power.value,ps1.power.alarm},"
    	+ "current{ps1.current.value,ps1.current.alarm},voltage{ps1.voltage.value,ps1.voltage.alarm}"
    	+ "})";
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "a{b{c{d}}}";
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "record[process=true,xxx=yyy]field(alarm,timeStamp[shareData=true],power.value";
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest==0);
    
    request = String("record[process=true,xxx=yyy]")
    	+ "putField(power.value)"
    	+ "getField(alarm,timeStamp,power{power.value,power.alarm},"
    	+ "current{current.value,current.alarm},voltage{voltage.value,voltage.alarm},"
    	+ "ps0{"
    	+ "ps0.alarm,ps0.timeStamp,power{ps0.power.value,ps0.power.alarm},"
    	+ "current{ps0.current.value,ps0.current.alarm},voltage{ps0.voltage.value,ps0.voltage.alarm}},"
    	+ "ps1{"
    	+ "ps1.alarm,ps1.timeStamp,power{ps1.power.value,ps1.power.alarm},"
    	+ "current{ps1.current.value,ps1.current.alarm},voltage{ps1.voltage.value,ps1.voltage.alarm}"
    	+ ")";
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest==0);

    request = "record[process=true,power.value";
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest==0);
    
    request = "field(power.value";
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest==0);
    
    request = "putField(power.value";
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest==0);

    request = "getField(power.value";
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest==0);

    request = "record[process=true=power.value]";
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = getCreateRequest()->createRequest(request,&requester);
    assert(pvRequest==0);

    printf("PASSED\n");

}

int main(int argc,char *argv[])
{
    testCreateRequest();

    std::cout << "-----------------------------------------------------------------------" << std::endl;
    getShowConstructDestruct()->constuctDestructTotals(stdout);
    return 0;
}



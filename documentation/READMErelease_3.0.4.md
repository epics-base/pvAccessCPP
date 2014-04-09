release/3.0.4
=============

This branch is intended for making pvAccessCPP changes related to EPICS Version 4 release 4.3.0.
The version  of pvAccessCPP for that release had a tag 3.0.4.
This branch was created relative to that tag.

rpcClient
--------

The original rpcClient had two problems:
1) It never connected.
2) It always blocks until a connect and request is complete.

The new version allows a client to connect to multiple services in parallel
and to make multiple requests in parallel.
An example is:

	RPCClientPtr rpc1 = RPCClient::create("service1",PVStructurePtr());
	RPCClientPtr rpc2 = RPCClient::create("service2",PVStructurePtr());
	rpc1->issueConnect();
	rpc2->issueConnect();
	if(!rpc1->waitConnect()) { /* take some action */}
	if(!rpc2->waitConnect()) { /* take some action */}
	rpc1->issueRequest(pvArgument1,false);
	rpc2->issueRequest(pvArgument2,false);
	PVStructurePtr pvResult1 = rpc1->waitRequest(1.0);
	PVStructurePtr pvResult2 = rpc2->waitRequest(1.0);

The client can also make one blocking call to make a request:

	PVStructurePtr pvResult = RPCClient::request(serviceName,pvArgument,1.0);

This will throw an exception if any errors occur.
 

/*
 * referencedTransportSender.h
 */

#ifndef REFERENCEDTRANSPORTSENDER_H_
#define REFERENCEDTRANSPORTSENDER_H_

#include "remote.h"

namespace epics {
namespace pvAccess {

class ReferencedTransportSender :  public TransportSender
{
public:
	ReferencedTransportSender();
	virtual ~ReferencedTransportSender();
	void release();
	void acquire();
private:
	epics::pvData::Mutex _refMutex;
	int32 _refCount;
};

}
}


#endif /* REFERENCEDTRANSPORTSENDER_H_ */

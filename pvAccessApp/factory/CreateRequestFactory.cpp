/*CreateRequestFactory.cpp*/
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvDataCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#include <string>
#include <sstream>
#include <pv/pvData.h>
#include <pv/lock.h>
#include <pv/pvAccess.h>

using namespace epics::pvData;


namespace epics { namespace pvAccess {

class CreateRequestImpl : public CreateRequest {
private:

	static void trim(std::string& str)
	{
		std::string::size_type pos = str.find_last_not_of(' ');
		if(pos != std::string::npos) {
			str.erase(pos + 1);
			pos = str.find_first_not_of(' ');
			if(pos != std::string::npos) str.erase(0, pos);
		}
		else str.erase(str.begin(), str.end());
	}

	static size_t findMatchingBrace(std::string& request, int index, int numOpen) {
		size_t openBrace = request.find('{', index+1);
		size_t closeBrace = request.find('}', index+1);
		if(openBrace == std::string::npos && closeBrace == std::string::npos) return std::string::npos;
		if (openBrace != std::string::npos) {
			if(openBrace<closeBrace) return findMatchingBrace(request,openBrace,numOpen+1);
			if(numOpen==1) return closeBrace;
			return findMatchingBrace(request,closeBrace,numOpen-1);
		}
		if(numOpen==1) return closeBrace;
		return findMatchingBrace(request,closeBrace,numOpen-1);
	}

	static void createFieldRequest(PVStructure* pvParent,std::string request,bool fieldListOK) {
		trim(request);
		if(request.length()<=0) return;
		size_t comma = request.find(',');
		size_t openBrace = request.find('{');
		size_t openBracket = request.find('[');
		if(openBrace != std::string::npos || openBracket != std::string::npos) fieldListOK = false;
		if(openBrace != std::string::npos && (comma==std::string::npos || comma>openBrace)) {
			//find matching brace
			size_t closeBrace = findMatchingBrace(request,openBrace+1,1);
			if(closeBrace==std::string::npos) {
				THROW_BASE_EXCEPTION("mismatched { }");
			}
			String fieldName = request.substr(0,openBrace);
			std::auto_ptr<PVStructure> pvStructure(getPVDataCreate()->createPVStructure(pvParent, fieldName, 0));
			createFieldRequest(pvStructure.get(),request.substr(openBrace+1,closeBrace-openBrace-1),false);
			pvParent->appendPVField(pvStructure.release());
			if(request.length()>closeBrace+1) {
				if(request.at(closeBrace+1) != ',') {
					THROW_BASE_EXCEPTION("misssing , after }");
				}
				createFieldRequest(pvParent,request.substr(closeBrace+2),false);
			}
			return;
		}
		if(openBracket==std::string::npos && fieldListOK) {
			std::auto_ptr<PVString> pvStringField(static_cast<PVString*>(getPVDataCreate()->createPVScalar(pvParent, "fieldList", pvString)));
			pvStringField->put(request);
			pvParent->appendPVField(pvStringField.release());
			return;
		}
		if(openBracket!=std::string::npos && (comma==std::string::npos || comma>openBracket)) {
			size_t closeBracket = request.find(']');
			if(closeBracket==std::string::npos) {
				THROW_BASE_EXCEPTION("option does not have matching []");
			}
			createLeafFieldRequest(pvParent,request.substr(0, closeBracket+1));
			size_t commaLoc = request.rfind(',');
			if(commaLoc!=std::string::npos && commaLoc>closeBracket) {
				int nextComma = request.find(',', closeBracket);
				createFieldRequest(pvParent,request.substr(nextComma+1),false);
			}
			return;
		}
		if(comma!=std::string::npos) {
			createLeafFieldRequest(pvParent,request.substr(0, comma));
			createFieldRequest(pvParent,request.substr(comma+1),false);
			return;
		}
		createLeafFieldRequest(pvParent,request);
	}

	static void createLeafFieldRequest(PVStructure* pvParent,String request) {
		size_t openBracket = request.find('[');
		String fullName = request;
		if(openBracket != std::string::npos) fullName = request.substr(0,openBracket);
		size_t indLast = fullName.rfind('.');
		String fieldName = fullName;
		if(indLast>1 && indLast != std::string::npos) fieldName = fullName.substr(indLast+1);
		std::auto_ptr<PVStructure> pvStructure(getPVDataCreate()->createPVStructure(pvParent, fieldName, 0));
		std::auto_ptr<PVStructure> pvLeaf(getPVDataCreate()->createPVStructure(pvStructure.get(),"leaf", 0));
		std::auto_ptr<PVString> pvStringField(static_cast<PVString*>(getPVDataCreate()->createPVScalar(pvLeaf.get(), "source", pvString)));
		pvStringField->put(fullName);
		pvLeaf->appendPVField(pvStringField.release());
		if(openBracket != std::string::npos) {
			size_t closeBracket = request.find(']');
			if(closeBracket==std::string::npos) {
				THROW_BASE_EXCEPTION("option does not have matching []");
			}
			createRequestOptions(pvLeaf.get(),request.substr(openBracket+1, closeBracket-openBracket-1));
		}
		pvStructure->appendPVField(pvLeaf.release());
		pvParent->appendPVField(pvStructure.release());
	}

	static void createRequestOptions(PVStructure* pvParent,std::string request) {
		trim(request);
		if(request.length()<=1) return;

		std::string token;
		std::istringstream iss(request);
		while (getline(iss, token, ','))
		{
			size_t equalsPos = token.find('=');
			size_t equalsRPos = token.rfind('=');
			if (equalsPos != equalsRPos)
			{
				THROW_BASE_EXCEPTION("illegal option");
			}

			if (equalsPos != std::string::npos)
			{
				std::auto_ptr<PVString> pvStringField(static_cast<PVString*>(getPVDataCreate()->createPVScalar(pvParent, token.substr(0, equalsPos), pvString)));
				pvStringField->put(token.substr(equalsPos+1));
				pvParent->appendPVField(pvStringField.release());
			}
		}
	}

public:

	virtual PVStructure::shared_pointer createRequest(String request)
	{
		static String emptyString;
		if (!request.empty()) trim(request);
		if (request.empty())
		{
			PVStructure::shared_pointer pvStructure(getPVDataCreate()->createPVStructure(0, emptyString, 0));
			return pvStructure;
		}

		size_t offsetRecord = request.find("record[");
		size_t offsetField = request.find("field(");
		size_t offsetPutField = request.find("putField(");
		size_t offsetGetField = request.find("getField(");

		PVStructure::shared_pointer pvStructure(getPVDataCreate()->createPVStructure(0, emptyString, 0));

		if (offsetRecord != std::string::npos) {
			size_t offsetBegin = request.find('[', offsetRecord);
			size_t offsetEnd = request.find(']', offsetBegin);
			if(offsetEnd == std::string::npos) {
				THROW_BASE_EXCEPTION("record[ does not have matching ]");
			}
			std::auto_ptr<PVStructure> pvStruct(getPVDataCreate()->createPVStructure(pvStructure.get(), "record", 0));
			createRequestOptions(pvStruct.get(),request.substr(offsetBegin+1, offsetEnd-offsetBegin-1));
			pvStructure->appendPVField(pvStruct.release());
		}
		if (offsetField != std::string::npos) {
			size_t offsetBegin = request.find('(', offsetField);
			size_t offsetEnd = request.find(')', offsetBegin);
			if(offsetEnd == std::string::npos) {
				THROW_BASE_EXCEPTION("field( does not have matching )");
			}
			std::auto_ptr<PVStructure> pvStruct(getPVDataCreate()->createPVStructure(pvStructure.get(), "field", 0));
			createFieldRequest(pvStruct.get(),request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),true);
			pvStructure->appendPVField(pvStruct.release());
		}
		if (offsetPutField != std::string::npos) {
			size_t offsetBegin = request.find('(', offsetPutField);
			size_t offsetEnd = request.find(')', offsetBegin);
			if(offsetEnd == std::string::npos) {
				THROW_BASE_EXCEPTION("putField( does not have matching )");
			}
			std::auto_ptr<PVStructure> pvStruct(getPVDataCreate()->createPVStructure(pvStructure.get(), "putField", 0));
			createFieldRequest(pvStruct.get(),request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),true);
			pvStructure->appendPVField(pvStruct.release());
		}
		if (offsetGetField != std::string::npos) {
			size_t offsetBegin = request.find('(', offsetGetField);
			size_t offsetEnd = request.find(')', offsetBegin);
			if(offsetEnd == std::string::npos) {
				THROW_BASE_EXCEPTION("getField( does not have matching )");
			}
			std::auto_ptr<PVStructure> pvStruct(getPVDataCreate()->createPVStructure(pvStructure.get(), "getField", 0));
			createFieldRequest(pvStruct.get(),request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),true);
			pvStructure->appendPVField(pvStruct.release());
		}
		if (pvStructure.get()->getStructure()->getNumberFields()==0) {
			createFieldRequest(pvStructure.get(),request,true);
		}
		return pvStructure;
	}

};

CreateRequest::shared_pointer createRequest;

CreateRequest::shared_pointer getCreateRequest() {
    static Mutex mutex;
    Lock guard(mutex);

    if(createRequest.get()==0){
        createRequest.reset(new CreateRequestImpl());
    }
    return createRequest;
}

}}


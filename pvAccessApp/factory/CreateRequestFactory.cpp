/*CreateRequestFactory.cpp*/
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvDataCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#include <string>
#include <sstream>
#include <pvData.h>
#include <lock.h>
#include <pvAccess.h>

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
    
    	static int findMatchingBrace(std::string& request, int index, int numOpen) {
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
    	
        static bool createFieldRequest(PVStructure* pvParent,std::string request,bool fieldListOK,Requester* requester) {
        	trim(request);
        	if(request.length()<=0) return true;
        	size_t comma = request.find(',');
        	size_t openBrace = request.find('{');
        	size_t openBracket = request.find('[');
        	if(openBrace != std::string::npos || openBracket != std::string::npos) fieldListOK = false;
        	if(openBrace != std::string::npos && (comma==std::string::npos || comma>openBrace)) {
        		//find matching brace
        		size_t closeBrace = findMatchingBrace(request,openBrace+1,1);
        		if(closeBrace==std::string::npos) {
        			requester->message(request + "mismatched { }", errorMessage);
        			return false;
        		}
        		String fieldName = request.substr(0,openBrace);
        		PVStructure* pvStructure = getPVDataCreate()->createPVStructure(pvParent, fieldName, 0);
        		createFieldRequest(pvStructure,request.substr(openBrace+1,closeBrace-openBrace-1),false,requester);
        		// error check? 
        		pvParent->appendPVField(pvStructure);
        		if(request.length()>closeBrace+1) {
        			if(request.at(closeBrace+1) != ',') {
        				requester->message(request + "misssing , after }", errorMessage);
        				return false;
        			}
        			if(!createFieldRequest(pvParent,request.substr(closeBrace+2),false,requester)) return false;
        		}
        		return true;
        	}
        	if(openBracket==std::string::npos && fieldListOK) {
        			PVString* pvStringField = static_cast<PVString*>(getPVDataCreate()->createPVScalar(pvParent, "fieldList", pvString));
        			pvStringField->put(request);
        			pvParent->appendPVField(pvStringField);
        			return true;
        	}
        	if(openBracket!=std::string::npos && (comma==std::string::npos || comma>openBracket)) {
        		size_t closeBracket = request.find(']');
    			if(closeBracket==std::string::npos) {
        		    requester->message(request + "option does not have matching []", errorMessage);
        			return false;
    			}
    			if(!createLeafFieldRequest(pvParent,request.substr(0, closeBracket+1),requester)) return false;
                        size_t commaLoc = request.rfind(',');
                        if(commaLoc!=std::string::npos && commaLoc>closeBracket) {
    				int nextComma = request.find(',', closeBracket);
    				if(!createFieldRequest(pvParent,request.substr(nextComma+1),false,requester)) return false;
    			} 
    			return true;
        	}
        	if(comma!=std::string::npos) {
        		if(!createLeafFieldRequest(pvParent,request.substr(0, comma),requester)) return false;
        		return createFieldRequest(pvParent,request.substr(comma+1),false,requester);
        	}
        	return createLeafFieldRequest(pvParent,request,requester);
        }
        
        static bool createLeafFieldRequest(PVStructure* pvParent,String request,Requester* requester) {
        	size_t openBracket = request.find('[');
        	String fullName = request;
        	if(openBracket != std::string::npos) fullName = request.substr(0,openBracket);
        	size_t indLast = fullName.rfind('.');
    		String fieldName = fullName;
    		if(indLast>1 && indLast != std::string::npos) fieldName = fullName.substr(indLast+1);
        	PVStructure* pvStructure = getPVDataCreate()->createPVStructure(pvParent, fieldName, 0);
    		PVStructure* pvLeaf = getPVDataCreate()->createPVStructure(pvStructure,"leaf", 0);
    		PVString* pvStringField = static_cast<PVString*>(getPVDataCreate()->createPVScalar(pvLeaf, "source", pvString));
    		pvStringField->put(fullName);
    		pvLeaf->appendPVField(pvStringField);
    		if(openBracket != std::string::npos) {
    			size_t closeBracket = request.find(']');
    			if(closeBracket==std::string::npos) {
    			    delete pvLeaf;
    			    delete pvStructure;
    				requester->message("option does not have matching []", errorMessage);
    				return false;
    			}
    			if(!createRequestOptions(pvLeaf,request.substr(openBracket+1, closeBracket-openBracket-1),requester))
    			{
    			    delete pvLeaf;
    			    delete pvStructure;
    			    return false;
    			}
    		}
    		pvStructure->appendPVField(pvLeaf);
    		pvParent->appendPVField(pvStructure);
    		return true;
        }
        
        static bool createRequestOptions(PVStructure* pvParent,std::string request,Requester* requester) {
    		trim(request);
    		if(request.length()<=1) return true;
    		
            std::string token;
            std::istringstream iss(request);
            while (getline(iss, token, ','))
            {
                size_t equalsPos = token.find('=');
                size_t equalsRPos = token.rfind('=');
                if (equalsPos != equalsRPos)
                {
        			requester->message("illegal option", errorMessage);
        			return false;
                }
                
                if (equalsPos != std::string::npos)
                {
            		PVString* pvStringField = static_cast<PVString*>(getPVDataCreate()->createPVScalar(pvParent, token.substr(0, equalsPos), pvString));
            		pvStringField->put(token.substr(equalsPos+1));
            		pvParent->appendPVField(pvStringField);
                }
            }
        	return true;
        }
    
    public:
        
    	virtual PVStructure* createRequest(String request, Requester* requester)
    	{
        	static String emptyString;
    		if (!request.empty()) trim(request);
        	if (request.empty())
        	{
        	   return getPVDataCreate()->createPVStructure(0, emptyString, 0);
        	}

    		size_t offsetRecord = request.find("record[");
    		size_t offsetField = request.find("field(");
    		size_t offsetPutField = request.find("putField(");
    		size_t offsetGetField = request.find("getField(");

            PVStructure* pvStructure = getPVDataCreate()->createPVStructure(0, emptyString, 0);

    		if (offsetRecord != std::string::npos) {
    			size_t offsetBegin = request.find('[', offsetRecord);
    			size_t offsetEnd = request.find(']', offsetBegin);
    			if(offsetEnd == std::string::npos) {
                    delete pvStructure;
    				requester->message("record[ does not have matching ]", errorMessage);
    				return 0;
    			}
    			PVStructure* pvStruct = getPVDataCreate()->createPVStructure(pvStructure, "record", 0);
    			if(!createRequestOptions(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),requester)) 
    			{
    			     delete pvStruct;
    			     delete pvStructure;
    			     return 0;
    			}
    			pvStructure->appendPVField(pvStruct);
    		}
    		if (offsetField != std::string::npos) {
    			size_t offsetBegin = request.find('(', offsetField);
    			size_t offsetEnd = request.find(')', offsetBegin);
    			if(offsetEnd == std::string::npos) {
                    delete pvStructure;
    				requester->message("field( does not have matching )", errorMessage);
    				return 0;
    			}
    			PVStructure* pvStruct = getPVDataCreate()->createPVStructure(pvStructure, "field", 0);
    			if(!createFieldRequest(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),true,requester))
    			{
    			     delete pvStruct;
    			     delete pvStructure;
    			     return 0;
    			}
     			pvStructure->appendPVField(pvStruct);
    		}
    		if (offsetPutField != std::string::npos) {
    			size_t offsetBegin = request.find('(', offsetPutField);
    			size_t offsetEnd = request.find(')', offsetBegin);
    			if(offsetEnd == std::string::npos) {
    			    delete pvStructure;
    				requester->message("putField( does not have matching )", errorMessage);
    				return 0;
    			}
    			PVStructure* pvStruct = getPVDataCreate()->createPVStructure(pvStructure, "putField", 0);
    			if(!createFieldRequest(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),true,requester))
    			{
    			     delete pvStruct;
    			     delete pvStructure;
    			     return 0;
    			}
    			pvStructure->appendPVField(pvStruct);
    		}
    		if (offsetGetField != std::string::npos) {
    			size_t offsetBegin = request.find('(', offsetGetField);
    			size_t offsetEnd = request.find(')', offsetBegin);
    			if(offsetEnd == std::string::npos) {
    			     delete pvStructure;
    				 requester->message("getField( does not have matching )", errorMessage);
    				 return 0;
    			}
    			PVStructure* pvStruct = getPVDataCreate()->createPVStructure(pvStructure, "getField", 0);
    			if(!createFieldRequest(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),true,requester)) 
    			{
    			     delete pvStruct;
    			     delete pvStructure;
    			     return 0;
    			}
    			pvStructure->appendPVField(pvStruct);
    		}
    		if (pvStructure->getStructure()->getNumberFields()==0) {
    			if(!createFieldRequest(pvStructure,request,true,requester))
    			{
    			     delete pvStructure;
    			     return 0;
    			}
    		}
        	return pvStructure;
    	}
    	
};

static CreateRequest* createRequest = 0;

CreateRequest * getCreateRequest() {
    static Mutex mutex = Mutex();
    Lock guard(&mutex);

    if(createRequest==0){
        createRequest = new CreateRequestImpl();
    }
    return createRequest;
}

}}


/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string>
#include <sstream>
#include <pv/pvData.h>
#include <pv/lock.h>
#include <pv/pvAccess.h>

using namespace epics::pvData;
using std::tr1::static_pointer_cast;



namespace epics {
namespace pvAccess {

static PVDataCreatePtr pvDataCreate = getPVDataCreate();

class CreateRequestImpl : public CreateRequest {
private:

    static void trim(String& str)
    {
        String::size_type pos = str.find_last_not_of(' ');
        if(pos != String::npos) {
            str.erase(pos + 1);
            pos = str.find_first_not_of(' ');
            if(pos != String::npos) str.erase(0, pos);
        }
        else str.erase(str.begin(), str.end());
    }

    static size_t findMatchingBrace(String& request, int index, int numOpen) {
        size_t openBrace = request.find('{', index+1);
        size_t closeBrace = request.find('}', index+1);
        if(openBrace == String::npos && closeBrace == std::string::npos) return std::string::npos;
        if (openBrace != String::npos && openBrace!=0) {
            if(openBrace<closeBrace) return findMatchingBrace(request,openBrace,numOpen+1);
            if(numOpen==1) return closeBrace;
            return findMatchingBrace(request,closeBrace,numOpen-1);
        }
        if(numOpen==1) return closeBrace;
        return findMatchingBrace(request,closeBrace,numOpen-1);
    }

    static std::vector<String> split(String const & commaSeparatedList) {
        String::size_type numValues = 1;
        String::size_type index=0;
        while(true) {
            String::size_type pos = commaSeparatedList.find(',',index);
            if(pos==String::npos) break;
            numValues++;
            index = pos +1;
        }
        std::vector<String> valueList(numValues,"");
        index=0;
        for(size_t i=0; i<numValues; i++) {
            size_t pos = commaSeparatedList.find(',',index);
            String value = commaSeparatedList.substr(index,pos-index);
            valueList[i] = value;
            index = pos +1;
        }
        return valueList;
    }


    static bool createRequestOptions(
        PVStructurePtr const & pvParent,
        String request,
        Requester::shared_pointer const & requester)
    {
        trim(request);
        if(request.length()<=1) return true;
        std::vector<String> items = split(request);
        size_t nitems = items.size();
        StringArray fieldNames;
        PVFieldPtrArray pvFields;
        fieldNames.reserve(nitems);
        pvFields.reserve(nitems);
        for(size_t j=0; j<nitems; j++) {
            String item = items[j];
            size_t equals = item.find('=');
            if(equals==String::npos || equals==0) {
                requester->message(item + " illegal option", errorMessage);

                return false;
            }
            String name = item.substr(0,equals);
            String value = item.substr(equals+1);
            fieldNames.push_back(name);
            PVStringPtr pvValue = static_pointer_cast<PVString>(getPVDataCreate()->createPVScalar(pvString));
            pvValue->put(value);
            pvFields.push_back(pvValue);
        }
        PVStructurePtr pvOptions = getPVDataCreate()->createPVStructure(fieldNames,pvFields);
        pvParent->appendPVField("_options",pvOptions);
        return true;
    }

    static bool createFieldRequest(
        PVStructurePtr const & pvParent,
        String request,
        Requester::shared_pointer const & requester)
    {
        static PVFieldPtrArray emptyFields;
        static StringArray emptyFieldNames;

        trim(request);
        if(request.length()<=0) return true;
        size_t comma = request.find(',');
        if(comma==0) {
            return createFieldRequest(pvParent,request.substr(1),requester);
        }
        size_t openBrace = request.find('{');
        size_t openBracket = request.find('[');
        PVStructurePtr pvStructure = pvDataCreate->createPVStructure(emptyFieldNames, emptyFields);
        if(comma==String::npos && openBrace==std::string::npos && openBracket==std::string::npos) {
            size_t period = request.find('.');
            if(period!=String::npos && period!=0) {
                String fieldName = request.substr(0,period);
                request = request.substr(period+1);
                pvParent->appendPVField(fieldName, pvStructure);
                return createFieldRequest(pvStructure,request,requester);
            }
            pvParent->appendPVField(request, pvStructure);
            return true;
        }
        size_t end = comma;
        if(openBrace!=String::npos && (end>openBrace || end==std::string::npos)) end = openBrace;
        if(openBracket!=String::npos && (end>openBracket || end==std::string::npos)) end = openBracket;
        String nextFieldName = request.substr(0,end);
        if(end==comma) {
            size_t period = nextFieldName.find('.');
            if(period!=String::npos && period!=0) {
                String fieldName = nextFieldName.substr(0,period);
                PVStructurePtr xxx= pvDataCreate->createPVStructure(emptyFieldNames, emptyFields);
                String rest = nextFieldName.substr(period+1);
                createFieldRequest(xxx,rest,requester);
                pvParent->appendPVField(fieldName, xxx);
            } else {
                pvParent->appendPVField(nextFieldName, pvStructure);
            }
            request = request.substr(end+1);
            return createFieldRequest(pvParent,request,requester);
        }
        if(end==openBracket) {
            size_t closeBracket =  request.find(']');
            if(closeBracket==String::npos || closeBracket==0) {
                requester->message(request + " does not have matching ]", errorMessage);
                return false;
            }
            String options = request.substr(openBracket+1, closeBracket-openBracket-1);
            size_t period = nextFieldName.find('.');
            if(period!=String::npos && period!=0) {
                String fieldName = nextFieldName.substr(0,period);
                PVStructurePtr xxx = pvDataCreate->createPVStructure(emptyFieldNames, emptyFields);
                if(!createRequestOptions(xxx,options,requester)) return false;
                String rest = nextFieldName.substr(period+1);
                createFieldRequest(xxx,rest,requester);
                pvParent->appendPVField(fieldName, xxx);
            } else {
                if(!createRequestOptions(pvStructure,options,requester)) return false;
                pvParent->appendPVField(nextFieldName, pvStructure);
            }
            request = request.substr(end+1);
            return createFieldRequest(pvParent,request,requester);
        }
        // end== openBrace
        size_t closeBrace = findMatchingBrace(request,openBrace+1,1);
        if(closeBrace==String::npos || closeBrace==0) {
            requester->message(request + " does not have matching }", errorMessage);
            return false;
        }
        String subFields = request.substr(openBrace+1, closeBrace-openBrace-1);
        if(!createFieldRequest(pvStructure,subFields,requester)) return false;
        request = request.substr(closeBrace+1);
        size_t period = nextFieldName.find('.');
        if(period==String::npos) {
            pvParent->appendPVField(nextFieldName,pvStructure);
            return createFieldRequest(pvParent,request,requester);
        }
        PVStructure::shared_pointer yyy = pvParent;
        while(period!=String::npos && period!=0) {
            String fieldName = nextFieldName.substr(0,period);
            PVStructurePtr xxx = pvDataCreate->createPVStructure(emptyFieldNames, emptyFields);
            yyy->appendPVField(fieldName,xxx);
            nextFieldName = nextFieldName.substr(period+1);
            period = nextFieldName.find('.');
            if(period==String::npos || period==0) {
                xxx->appendPVField(nextFieldName, pvStructure);
                break;
            }
            yyy = xxx;
        }
        return createFieldRequest(pvParent,request,requester);
    }

public:

    virtual PVStructure::shared_pointer createRequest(
        String const & crequest,
        Requester::shared_pointer const &  requester)
    {
    	String request = crequest;
        PVFieldPtrArray pvFields;
        StringArray fieldNames;
        PVStructurePtr emptyPVStructure = pvDataCreate->createPVStructure(fieldNames,pvFields);
        static PVStructure::shared_pointer nullStructure;

        if (!request.empty()) trim(request);
        if (request.empty())
        {
            return emptyPVStructure;
        }
        size_t offsetRecord = request.find("record[");
        size_t offsetField = request.find("field(");
        size_t offsetPutField = request.find("putField(");
        size_t offsetGetField = request.find("getField(");
        PVStructurePtr pvStructure = pvDataCreate->createPVStructure(emptyPVStructure);
        if (offsetRecord != String::npos) {
            size_t offsetBegin = request.find('[', offsetRecord);
            size_t offsetEnd = request.find(']', offsetBegin);
            if(offsetEnd == String::npos) {
                requester->message(request.substr(offsetRecord) + " record[ does not have matching ]", errorMessage);
                return nullStructure;
            }
            PVStructurePtr pvStruct =  pvDataCreate->createPVStructure(emptyPVStructure);
            if(!createRequestOptions(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),requester)) {
                 return nullStructure;
            }
            pvStructure->appendPVField("record", pvStruct);
        }
        if (offsetField != String::npos) {
            size_t offsetBegin = request.find('(', offsetField);
            size_t offsetEnd = request.find(')', offsetBegin);
            if(offsetEnd == String::npos) {
                requester->message(request.substr(offsetField) + " field( does not have matching )", errorMessage);
                return nullStructure;
            }
            PVStructurePtr pvStruct =  pvDataCreate->createPVStructure(emptyPVStructure);
            if(!createFieldRequest(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),requester)) {
                return nullStructure;
            }
            pvStructure->appendPVField("field", pvStruct);
        }
        if (offsetPutField != String::npos) {
            size_t offsetBegin = request.find('(', offsetPutField);
            size_t offsetEnd = request.find(')', offsetBegin);
            if(offsetEnd == String::npos) {
                requester->message(request.substr(offsetField) + " putField( does not have matching )", errorMessage);
                return nullStructure;
            }
            PVStructurePtr pvStruct =  pvDataCreate->createPVStructure(emptyPVStructure);
            if(!createFieldRequest(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),requester)) {
                 return nullStructure;
            }
            pvStructure->appendPVField("putField", pvStruct);
        }
        if (offsetGetField != String::npos) {
            size_t offsetBegin = request.find('(', offsetGetField);
            size_t offsetEnd = request.find(')', offsetBegin);
            if(offsetEnd == String::npos) {
                requester->message(request.substr(offsetField) + " getField( does not have matching )", errorMessage);
                return nullStructure;
            }
            PVStructurePtr pvStruct =  pvDataCreate->createPVStructure(emptyPVStructure);
            if(!createFieldRequest(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),requester)) {
                return nullStructure;
            }
            pvStructure->appendPVField("getField", pvStruct);
        }
        if (pvStructure.get()->getStructure()->getNumberFields()==0) {
            if(!createFieldRequest(pvStructure,request,requester)) return nullStructure;
        }
        return pvStructure;
    }

};

static CreateRequest::shared_pointer createRequest;

CreateRequest::shared_pointer getCreateRequest() {
    static Mutex mutex;
    Lock guard(mutex);

    if(createRequest.get()==0){
        createRequest.reset(new CreateRequestImpl());
    }
    return createRequest;
}

}}


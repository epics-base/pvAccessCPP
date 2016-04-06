#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include <iostream>
#include <pv/clientFactory.h>
#include <pv/pvAccess.h>

#include <pv/caProvider.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsThread.h>
#include <pv/logger.h>
#include <pv/lock.h>

#include <vector>
#include <string>
#include <istream>
#include <ostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>

#include <pv/event.h>
#include <epicsExit.h>

#include "pvutils.cpp"

using namespace std;
namespace TR1 = std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

enum PrintMode { ValueOnlyMode, StructureMode, TerseMode };
PrintMode mode = ValueOnlyMode;

char fieldSeparator = ' ';

bool columnMajor = false;

bool transpose = false;

bool dumpStructure = false;

#if defined(_WIN32) && !defined(_MINGW)
FILE *popen(const char *command, const char *mode) {
    return _popen(command, mode);
}
int pclose(FILE *stream) {
    return _pclose(stream);
}
#endif

void formatNTAny(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVFieldPtr value = pvStruct->getSubField("value");
    if (value.get() == 0)
    {
        std::cerr << "no 'value' field in NTAny" << std::endl;
        return;
    }

    o << *value;
}

void formatNTScalar(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVScalarPtr value = TR1::dynamic_pointer_cast<PVScalar>(pvStruct->getSubField("value"));
    if (value.get() == 0)
    {
        std::cerr << "no scalar_t 'value' field in NTScalar" << std::endl;
        return;
    }

    o << *value;
}

std::ostream& formatVector(std::ostream& o,
                           string label,
                           PVScalarArrayPtr const & pvScalarArray,
                           bool transpose)
{
    size_t len = pvScalarArray->getLength();

    if (!transpose)
    {
        if (!label.empty())
            o << label << std::endl;

        for (size_t i = 0; i < len; i++)
            pvScalarArray->dumpValue(o, i) << std::endl;
    }
    else
    {
        bool first = true;
        if (!label.empty())
        {
            o << label;
            first = false;
        }

        for (size_t i = 0; i < len; i++) {
            if (first)
                first = false;
            else
                o << fieldSeparator;

            pvScalarArray->dumpValue(o, i);
        }
    }

    return o;
}

/*
std::ostream& formatScalarArray(std::ostream& o, PVScalarArrayPtr const & pvScalarArray)
{
    size_t len = pvScalarArray->getLength();
    if (len == 0)
    {
        // TODO do we really want this
        o << "(empty)" << std::endl;
    }
    else
    {
        for (size_t i = 0; i < len; i++)
            pvScalarArray->dumpValue(o, i) << std::endl;
    }
    return o;
}
*/

void formatNTScalarArray(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVScalarArrayPtr value = TR1::dynamic_pointer_cast<PVScalarArray>(pvStruct->getSubField("value"));
    if (value.get() == 0)
    {
        std::cerr << "no scalar_t[] 'value' field in NTScalarArray" << std::endl;
        return;
    }

    //o << *value;
    //formatScalarArray(o, value);
    formatVector(o, "", value, mode == TerseMode || transpose);
}

void formatNTEnum(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVStructurePtr enumt = TR1::dynamic_pointer_cast<PVStructure>(pvStruct->getSubField("value"));
    if (enumt.get() == 0)
    {
        std::cerr << "no enum_t 'value' field in NTEnum" << std::endl;
        return;
    }

    printEnumT(o, enumt);
}

size_t getLongestString(shared_vector<const string> const & array)
{
    size_t max = 0;
    size_t len = array.size();
    for (size_t i = 0; i < len; i++)
    {
        size_t l = array[i].size();
        if (l > max) max = l;
    }
    return max;
}

size_t getLongestString(PVScalarArrayPtr const & array)
{
    size_t max = 0;

    string empty;

    ostringstream oss;
    size_t len = array->getLength();
    for (size_t i = 0; i < len; i++)
    {
        oss.str(empty);
        array->dumpValue(oss, i);
        size_t l = oss.tellp();
        if (l > max) max = l;
    }
    return max;
}

// labels are optional
// if provided labels.size() must equals columnData.size()
void formatTable(std::ostream& o,
                 shared_vector<const string> const & labels,
                 vector<PVScalarArrayPtr> const & columnData,
                 bool showHeader, bool transpose)
{
    // array with maximum number of elements
    size_t maxValues = 0;

    // value with longest string form
    size_t maxLabelColumnLength = (showHeader && labels.size()) ? getLongestString(labels) : 0;
    size_t maxColumnLength = 0;
    //
    // get maxValue and maxColumnLength
    //
    size_t numColumns = columnData.size();
    for (size_t i = 0; i < numColumns; i++)
    {
        PVScalarArrayPtr array = columnData[i];
        if (array.get())
        {
            size_t arrayLength = array->getLength();
            if (maxValues < arrayLength) maxValues = arrayLength;

            size_t colLen = getLongestString(array);
            if (colLen > maxColumnLength) maxColumnLength = colLen;
        }
    }

    // add some space
    size_t padding = 2;
    maxColumnLength += padding;

    if (!transpose)
    {
        /* non-compact
        maxLabelColumnLength += padding;

        // increase maxColumnLength to maxLabelColumnLength
        if (maxLabelColumnLength > maxColumnLength)
            maxColumnLength = maxLabelColumnLength;
        */

        //
        // <column0>, <column1>, ...
        //   values     values   ...
        //

        // first print labels
        if (showHeader && labels.size())
        {
            for (size_t i = 0; i < numColumns; i++)
            {
                if (separator == ' ')
                {
                    int width = std::max(labels[i].size()+padding, maxColumnLength);
                    o << std::setw(width) << std::right;
                    // non-compact o << std::setw(maxColumnLength) << std::right;
                }
                else if (i > 0)
                {
                    o << separator;
                }

                o << labels[i];
            }
            o << std::endl;
        }

        // then values
        for (size_t r = 0; r < maxValues; r++)
        {
            for (size_t i = 0; i < numColumns; i++)
            {
                if (separator == ' ' && (showHeader || numColumns > 1))
                {
                    int width = std::max(labels[i].size()+padding, maxColumnLength);
                    o << setw(width) << std::right;
                    // non-compact o << std::setw(maxColumnLength) << std::right;
                }
                else if (i > 0)
                {
                    o << separator;
                }

                PVScalarArrayPtr array = columnData[i];
                if (array.get() && r < array->getLength())
                    array->dumpValue(o, r);
                else
                    o << "";
            }
            o << std::endl;
        }

    }
    else
    {

        //
        // <column0> values...
        // <column1> values...
        // ...
        //

        for (size_t i = 0; i < numColumns; i++)
        {
            if (showHeader && labels.size())
            {
                if (separator == ' ')
                {
                    o << std::setw(maxLabelColumnLength) << std::left;
                }
                o << labels[i];
            }

            for (size_t r = 0; r < maxValues; r++)
            {
                if (separator == ' ' && (showHeader || numColumns > 1))
                {
                    o << std::setw(maxColumnLength) << std::right;
                }
                else if (showHeader || r > 0)
                    o << separator;

                PVScalarArrayPtr array = columnData[i];
                if (array.get() && r < array->getLength())
                    array->dumpValue(o, r);
                else
                    o << "";
            }
            o << std::endl;
        }

    }
}

void formatNTTable(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVStringArrayPtr labels = pvStruct->getSubField<PVStringArray>("labels");
    if (labels.get() == 0)
    {
        std::cerr << "no string[] 'labels' field in NTTable" << std::endl;
        return;
    }

    PVStructurePtr value = pvStruct->getSubField<PVStructure>("value");
    if (value.get() == 0)
    {
        std::cerr << "no 'value' structure in NTTable" << std::endl;
        return;
    }

    vector<PVScalarArrayPtr> columnData;
    PVFieldPtrArray fields = value->getPVFields();
    size_t numColumns = fields.size();

    if (labels->getLength() != numColumns)
    {
        std::cerr << "malformed NTTable, length of 'labels' array does not equal to a number of 'value' structure subfields" << std::endl;
        return;
    }

    for (size_t i = 0; i < numColumns; i++)
    {
        PVScalarArrayPtr array = TR1::dynamic_pointer_cast<PVScalarArray>(fields[i]);
        if (array.get() == 0)
        {
            std::cerr << "malformed NTTable, " << (i+1) << ". field is not scalar_t[]" << std::endl;
            return;
        }

        columnData.push_back(array);
    }

    bool showHeader = (mode != TerseMode);
    formatTable(o, labels->view(), columnData, showHeader, transpose);
}


void formatNTMatrix(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVDoubleArrayPtr value = pvStruct->getSubField<PVDoubleArray>("value");
    if (value.get() == 0)
    {
        std::cerr << "no double[] 'value' field in NTMatrix" << std::endl;
        return;
    }

    int32 rows, cols;

    PVIntArrayPtr dim = pvStruct->getSubField<PVIntArray>("dim");
    if (dim.get() != 0)
    {
        // dim[] = { rows, columns }
        size_t dims = dim->getLength();
        if (dims != 1 && dims != 2)
        {
            std::cerr << "malformed NTMatrix, dim[] must contain 1 or 2 elements instead of  " << dims << std::endl;
            return;
        }

        PVIntArray::const_svector data = dim->view();
        rows = data[0];
        cols = (dims == 2) ? data[1] : 1;

        if (rows <= 0 || cols <= 0)
        {
            std::cerr << "malformed NTMatrix, dim[] must contain elements > 0" << std::endl;
            return;
        }
    }
    else
    {
        // column vector
        rows = value->getLength();
        cols = 1;
    }

    o << std::left;

    size_t len = static_cast<size_t>(rows*cols);
    if (len != value->getLength())
    {
        std::cerr << "malformed NTMatrix, values[] must contain " << len << " elements instead of  " << value->getLength() << std::endl;
        return;
    }

    // add some space
    size_t padding = 2;
    size_t maxColumnLength = getLongestString(value) + padding;

    if (!transpose)
    {

        //
        // el1 el2 el3
        // el4 el5 el6
        //

        size_t ix = 0;
        for (int32 r = 0; r < rows; r++)
        {
            for (int32 c = 0; c < cols; c++)
            {
                if (separator == ' ' && cols > 1)
                    o << std::setw(maxColumnLength) << std::right;
                else if (c > 0)
                    o << separator;

                if (columnMajor)
                    value->dumpValue(o, r + c * rows);
                else
                    value->dumpValue(o, ix++);
            }
            o << std::endl;
        }

    }
    else
    {
        //
        // el1 el4
        // el2 el5
        // el3 el6
        //
        size_t ix = 0;
        for (int32 c = 0; c < cols; c++)
        {
            for (int32 r = 0; r < rows; r++)
            {
                if (separator == ' ' && rows > 1)
                    o << std::setw(maxColumnLength) << std::right;
                else if (r > 0)
                    o << separator;

                if (columnMajor)
                    value->dumpValue(o, ix++);
                else
                    value->dumpValue(o, r * cols + c);
            }
            o << std::endl;
        }
    }
}


// TODO use formatNTTable
void formatNTNameValue(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVStringArrayPtr name = pvStruct->getSubField<PVStringArray>("name");
    if (name.get() == 0)
    {
        std::cerr << "no string[] 'name' field in NTNameValue" << std::endl;
        return;
    }

    PVFieldPtr value = pvStruct->getSubField("value");
    if (value.get() == 0)
    {
        std::cerr << "no 'value' field in NTNameValue" << std::endl;
        return;
    }

    PVScalarArrayPtr array = TR1::dynamic_pointer_cast<PVScalarArray>(value);
    if (array.get() == 0)
    {
        std::cerr << "malformed NTNameValue, 'value' field is not scalar_t[]" << std::endl;
        return;
    }

    if (name->getLength() != array->getLength())
    {
        std::cerr << "malformed NTNameValue, length of 'name' and 'value' array does not equal" << std::endl;
        return;
    }

    size_t numColumns = name->getLength();

    // get names
    PVStringArray::const_svector nameData = name->view();

    // get max column name size
    bool showHeader = (mode != TerseMode);

    size_t maxLabelColumnLength = showHeader ? getLongestString(nameData) : 0;
    size_t maxColumnLength = showHeader ? getLongestString(array) : 0;

    // add some space
    size_t padding = 2;
    maxColumnLength += padding;

    if (transpose)
    {
        /* non-compact
        maxLabelColumnLength += padding;

        // increase maxColumnLength to maxLabelColumnLength
        if (maxLabelColumnLength > maxColumnLength)
            maxColumnLength = maxLabelColumnLength;
        */

        //
        //   <name0>, <name1>, ...
        //   value     values   ...
        //

        // first print names
        if (showHeader)
        {
            for (size_t i = 0; i < numColumns; i++)
            {
                if (separator == ' ')
                {
                    int width = std::max(nameData[i].size()+padding, maxColumnLength);
                    o << std::setw(width) << std::right;
                    // non-compact o << std::setw(maxColumnLength) << std::right;
                }
                else if (i > 0)
                {
                    o << separator;
                }

                o << nameData[i];
            }
            o << std::endl;
        }

        // then values
        for (size_t i = 0; i < numColumns; i++)
        {
            if (separator == ' ' && showHeader)
            {
                int width = std::max(nameData[i].size()+padding, maxColumnLength);
                o << std::setw(width) << std::right;
                // non-compact o << std::setw(maxColumnLength) << std::right;
            }
            else if (i > 0)
            {
                o << separator;
            }
            array->dumpValue(o, i);
        }
        o << std::endl;
    }
    else
    {

        //
        // <name0> values...
        // <name1> values...
        // ...
        //

        for (size_t i = 0; i < numColumns; i++)
        {
            if (showHeader)
            {
                if (separator == ' ')
                {
                    o << std::setw(maxLabelColumnLength) << std::left;
                }
                o << nameData[i];
            }

            if (separator == ' ' && showHeader)
            {
                o << std::setw(maxColumnLength) << std::right;
            }
            else if (showHeader)
            {
                o << separator;
            }

            array->dumpValue(o, i);
            o << std::endl;
        }

    }

}

void formatNTURI(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVStringPtr scheme = TR1::dynamic_pointer_cast<PVString>(pvStruct->getSubField<PVString>("scheme"));
    if (scheme.get() == 0)
    {
        std::cerr << "no string 'scheme' field in NTURI" << std::endl;
    }

    PVStringPtr authority = TR1::dynamic_pointer_cast<PVString>(pvStruct->getSubField("authority"));

    PVStringPtr path = TR1::dynamic_pointer_cast<PVString>(pvStruct->getSubField<PVString>("path"));
    if (path.get() == 0)
    {
        std::cerr << "no string 'path' field in NTURI" << std::endl;
        return;
    }

    PVStructurePtr query = TR1::dynamic_pointer_cast<PVStructure>(pvStruct->getSubField("query"));

    o << scheme->get() << "://";
    if (authority.get()) o << authority->get();
    o << '/' << path->get();

    // query
    if (query.get())
    {
        PVFieldPtrArray fields = query->getPVFields();
        size_t numColumns = fields.size();

        if (numColumns > 0)
        {
            o << '?';

            for (size_t i = 0; i < numColumns; i++)
            {
                if (i)
                    o << '&';

                // TODO encode value!!!
                o << fields[i]->getFieldName() << '=' << *(fields[i].get());
            }
        }
    }

    o << std::endl;
}


void formatNTNDArray(std::ostream& /*o*/, PVStructurePtr const & pvStruct)
{

    PVUnion::shared_pointer pvUnion = pvStruct->getSubField<PVUnion>("value");
    if (pvUnion.get() == 0)
    {
        std::cerr << "no 'value' union field in NTNDArray" << std::endl;
        return;
    }

    PVScalarArray::shared_pointer value = pvUnion->get<PVScalarArray>();
    if (value.get() == 0)
    {
        std::cerr << "'value' union field in given NTNDArray does not hold scalar array value" << std::endl;
        return;
    }

    // undefined
    int32 cm = -1;

    PVStructureArray::shared_pointer pvAttributes = pvStruct->getSubField<PVStructureArray>("attribute");
    PVStructureArray::const_svector attributes(pvAttributes->view());

    for (PVStructureArray::const_svector::const_iterator iter = attributes.begin();
            iter != attributes.end();
            iter++)
    {
        PVStructure::shared_pointer attribute = *iter;
        PVString::shared_pointer pvName = attribute->getSubField<PVString>("name");
        if (pvName && pvName->get() == "ColorMode")
        {
            PVInt::shared_pointer pvCM = attribute->getSubField<PVUnion>("value")->get<PVInt>();
            if (!pvCM)
                break;

            cm = pvCM->get();
            break;
        }
    }

    if (cm == -1)
    {
        std::cerr << "no PVInt 'ColorMode' attribute in NTNDArray" << std::endl;
        return;
    }

    if (cm != 0 && cm != 1 && cm != 2)
    {
        std::cerr << "unsupported 'ColorMode', only {0,1,2} modes are supported" << std::endl;
        return;
    }


    int32 rows, cols;

    PVStructureArray::shared_pointer dim = pvStruct->getSubField<PVStructureArray>("dimension");
    if (dim.get() == 0)
    {
        std::cerr << "no 'dimension' structure array field in NTNDArray" << std::endl;
        return;
    }

    // dim[] = { rows, columns } or
    // dim[] = { 3, rows, columns }
    PVStructureArray::const_svector data = dim->view();
    size_t dims = dim->getLength();
    size_t imageSize;
    if ((cm == 0 || cm == 1) && dims == 2)
    {
        cols = data[0]->getSubField<PVInt>("size")->get();
        rows = data[1]->getSubField<PVInt>("size")->get();
        imageSize = cols * rows;
    }
    else if (cm == 2 && dims == 3)
    {
        cols = data[1]->getSubField<PVInt>("size")->get();
        rows = data[2]->getSubField<PVInt>("size")->get();
        imageSize = cols * rows * 3;
    }
    else
    {
        std::cerr << "malformed NTNDArray, dimension[] is invalid for specified color mode" << std::endl;
        return;
    }

    if (rows <= 0 || cols <= 0)
    {
        std::cerr << "malformed NTNDArray, dimension[] gives negative size" << std::endl;
        return;
    }

    PVByteArrayPtr array = TR1::dynamic_pointer_cast<PVByteArray>(value);
    if (array.get() == 0)
    {
        std::cerr << "currently only byte[] value is supported" << std::endl;
        return;
    }

    if (array->getLength() != imageSize)
    {
        std::cerr << "value array length does not match expected image size (" <<
                  array->getLength() << " != " << imageSize << ")" << std::endl;
        return;
    }

    PVByteArray::const_svector img = array->view();
    /*
    size_t len = static_cast<size_t>(rows*cols);
    for (size_t i = 0; i < len; i++)
        o << img[i];
    */
    //eget -s testImage | gnuplot  -e "set size ratio -1; set palette grey; set cbrange [0:255]; plot '-'  binary array=(512,384) flipy format='%uchar' with image"

    FILE* gnuplotPipe = popen ("gnuplot -persist", "w");   // use -persist for backward compatibility (to support older gnuplot versions)

    const char *prologue = getenv("EGET_GNUPLOT_PROLOGUE");
    if (prologue)
        fprintf(gnuplotPipe, "%s\n", prologue);

    fprintf(gnuplotPipe, "set format \"\"\n");
    fprintf(gnuplotPipe, "unset key\n");
    fprintf(gnuplotPipe, "unset border\n");
    fprintf(gnuplotPipe, "unset colorbox\n");
    fprintf(gnuplotPipe, "unset xtics\n");
    fprintf(gnuplotPipe, "unset ytics\n");

    fprintf(gnuplotPipe, "set size ratio 1\n");
    fprintf(gnuplotPipe, "set xrange [0:%u]\n", cols-1);
    fprintf(gnuplotPipe, "set yrange [0:%u]\n", rows-1);

    if (cm == 2)
    {
        // RGB

        fprintf(gnuplotPipe, "plot '-'  binary array=(%u,%u) flipy format='%%uchar' with rgbimage\n", cols, rows);

        for (size_t i = 0; i < imageSize; i++)
            fprintf(gnuplotPipe, "%c", img[i]);
    }
    else
    {
        // monochrome

        fprintf(gnuplotPipe, "set palette grey\n");
        fprintf(gnuplotPipe, "set cbrange [0:255]\n");

        fprintf(gnuplotPipe, "plot '-'  binary array=(%u,%u) flipy format='%%uchar' with image\n", cols, rows);

        for (size_t i = 0; i < imageSize; i++)
            fprintf(gnuplotPipe, "%c", img[i]);
    }

    fflush(gnuplotPipe);
    pclose(gnuplotPipe);

}

typedef void(*NTFormatterFunc)(std::ostream& o, PVStructurePtr const & pvStruct);
typedef map<string, NTFormatterFunc> NTFormatterLUTMap;
NTFormatterLUTMap ntFormatterLUT;

void initializeNTFormatterLUT()
{
    ntFormatterLUT["epics:nt/NTScalar:1"] = formatNTScalar;
    ntFormatterLUT["epics:nt/NTScalarArray:1"] = formatNTScalarArray;
    ntFormatterLUT["epics:nt/NTEnum:1"] = formatNTEnum;
    ntFormatterLUT["epics:nt/NTTable:1"] = formatNTTable;
    ntFormatterLUT["epics:nt/NTMatrix:1"] = formatNTMatrix;
    ntFormatterLUT["epics:nt/NTAny:1"] = formatNTAny;
    ntFormatterLUT["epics:nt/NTNameValue:1"] = formatNTNameValue;
    ntFormatterLUT["epics:nt/NTURI:1"] = formatNTURI;
    ntFormatterLUT["epics:nt/NTNDArray:1"] = formatNTNDArray;
}

void formatNT(std::ostream& o, PVFieldPtr const & pv)
{
    static bool lutInitialized = false;
    if (!lutInitialized)
    {
        initializeNTFormatterLUT();
        lutInitialized = true;
    }

    if (pv.get() == 0)
    {
        o << "(null)" << std::endl;
        return;
    }

    Type type = pv->getField()->getType();
    if (type==structure)
    {
        PVStructurePtr pvStruct = TR1::static_pointer_cast<PVStructure>(pv);
        {
            string id = pvStruct->getField()->getID();

            // remove minor
            size_t pos = id.find_last_of('.');
            if (pos != string::npos)
                id = id.substr(0, pos);

            NTFormatterLUTMap::const_iterator formatter = ntFormatterLUT.find(id);
            if (formatter != ntFormatterLUT.end())
            {
                (formatter->second)(o, pvStruct);
            }
            else
            {
                std::cerr << "non-normative type" << std::endl;
                //o << *(pv.get()) << std::endl;
                pvutil_ostream myos(std::cout.rdbuf());
                myos << *(pv.get()) << std::endl;
            }

            return;
        }
    }

    // no ID, just dump
    pvutil_ostream myos(std::cout.rdbuf());
    myos << *(pv.get()) << std::endl;
}

void dumpValue(std::string const & channelName, PVField::shared_pointer const & pv)
{
    if (!channelName.empty())
        std::cout << channelName << std::endl;
    //std::cout << *(pv.get()) << std::endl << std::endl;

    pvutil_ostream myos(std::cout.rdbuf());
    if (pv->getField()->getType() == structure)
        myos << *(TR1::static_pointer_cast<PVStructure>(pv).get()) << std::endl << std::endl;
    else
        myos << *(pv.get()) << std::endl << std::endl;
}

void printValue(std::string const & channelName, PVStructure::shared_pointer const & pv, bool forceTerseWithName = false)
{
    if (forceTerseWithName)
    {
        if (!channelName.empty())
            std::cout << channelName << separator;
        terseStructure(std::cout, pv) << std::endl;
    }
    else if (mode == ValueOnlyMode)
    {
        PVField::shared_pointer value = pv->getSubField("value");
        if (value.get() == 0)
        {
            std::cerr << "no 'value' field" << std::endl;
            dumpValue(channelName, pv);
        }
        else
        {
            Type valueType = value->getField()->getType();
            if (valueType == scalar)
                std::cout << *(value.get()) << std::endl;
            else if (valueType == scalarArray)
            {
                //formatScalarArray(std::cout, TR1::dynamic_pointer_cast<PVScalarArray>(value));
                formatVector(std::cout, "", TR1::dynamic_pointer_cast<PVScalarArray>(value), false);
            }
            else
            {
                // switch to structure mode, unless it's T-type
                if (valueType == structure && isTType(TR1::static_pointer_cast<PVStructure>(value)))
                {
                    formatTType(std::cout, TR1::static_pointer_cast<PVStructure>(value));
                    std::cout << std::endl;
                }
                else
                    dumpValue(channelName, pv);
            }
        }
    }
    else if (mode == TerseMode)
        terseStructure(std::cout, pv) << std::endl;
    else
        dumpValue(channelName, pv);
}

static string emptyString;

// only in ValueOnlyMode
// NOTE: names might be empty
void printValues(shared_vector<const string> const & names, vector<PVStructure::shared_pointer> const & values)
{
    size_t len = values.size();

    vector<PVScalar::shared_pointer> scalars;
    vector<PVScalarArray::shared_pointer> scalarArrays;

    for (size_t i = 0; i < len; i++)
    {
        PVField::shared_pointer value = values[i]->getSubField("value");
        if (value.get() != 0)
        {
            Type type = value->getField()->getType();
            if (type == scalarArray)
                scalarArrays.push_back(TR1::dynamic_pointer_cast<PVScalarArray>(value));
            else if (type == scalar)
            {
                PVScalar::shared_pointer scalar = TR1::dynamic_pointer_cast<PVScalar>(value);
                scalars.push_back(scalar);

                // make an array, i.e. PVStringArray, out of a scalar (since scalar is an array w/ element count == 1)
                PVStringArray::shared_pointer StringArray =
                    TR1::dynamic_pointer_cast<PVStringArray>(getPVDataCreate()->createPVScalarArray(pvString));

                PVStringArray::svector values;
                values.push_back(scalar->getAs<std::string>());
                StringArray->replace(freeze(values));

                scalarArrays.push_back(StringArray);
            }
        }
        else
        {
            // a structure detected, break
            break;
        }
    }

    if (scalars.size() == len)
    {
        if (!transpose)
        {
            bool first = true;
            for (size_t i = 0; i < len; i++)
            {
                if (first)
                    first = false;
                else
                    std::cout << fieldSeparator;
                std::cout << *(scalars[i].get());
            }
            std::cout << std::endl;
        }
        else
        {
            for (size_t i = 0; i < len; i++)
                std::cout << *(scalars[i].get()) << std::endl;
        }
    }
    else if (scalarArrays.size() == len)
    {
        // format as a table
        bool showHeader = false; //(mode != TerseMode);
        formatTable(std::cout, names, scalarArrays, showHeader, transpose);
    }
    else
    {
        // force terse mode with names
        for (size_t i = 0; i < len; i++)
            printValue(names[i], values[i], true);
    }
}

#define DEFAULT_TIMEOUT 3.0
#define DEFAULT_REQUEST "field(value)"
#define DEFAULT_RPC_REQUEST ""
#define DEFAULT_PROVIDER "pva"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);


void usage (void)
{
    fprintf (stderr, "\nUsage: eget [options] [<PV name>... | -s <service name>]\n\n"
             "  -h: Help: Print this message\n"
             "  -v: Print version and exit\n"
             "\noptions:\n"
             "  -s <service name>:   Service API compliant based RPC service name (accepts NTURI request argument)\n"
             "  -a <service arg>:    Service argument in 'name[=value]' or 'name value' form\n"
             "  -r <pv request>:     Get request string, specifies what fields to return and options, default is '%s'\n"
             "  -w <sec>:            Wait time, specifies timeout, default is %f second(s)\n"
             "  -z:                  Pure pvAccess RPC based service (send NTURI.query as request argument)\n"
             "  -N:                  Do not format NT types, dump structure instead\n"
             "  -i:                  Do not format standard types (enum_t, time_t, ...)\n"
             "  -t:                  Terse mode\n"
             "  -T:                  Transpose vector, table, matrix\n"
             "  -m:                  Monitor mode\n"
             "  -x:                  Use column-major order to decode matrix\n"
             "  -p <provider>:       Set default provider name, default is '%s'\n"
             "  -q:                  Quiet mode, print only error messages\n"
             "  -d:                  Enable debug output\n"
             "  -F <ofs>:            Use <ofs> as an alternate output field separator\n"
             "  -f <input file>:     Use <input file> as an input that provides a list PV name(s) to be read, use '-' for stdin\n"
             "  -c:                  Wait for clean shutdown and report used instance count (for expert users)\n"
             " enum format:\n"
             "  -n: Force enum interpretation of values as numbers (default is enum string)\n"
             "\n\nexamples:\n\n"
             "#! Get the value of the PV corr:li32:53:bdes\n"
             "> eget corr:li32:53:bdes\n"
             "\n"
             "#! Get the table of all correctors from the rdb service\n"
             "> eget -s rdbService -a entity=swissfel:devicenames\n"
             "\n"
             "#! Get the archive history of quad45:bdes;history between 2 times, from the archive service\n"
             "> eget -s archiveService -a entity=quad45:bdes;history -a starttime=2012-02-12T10:04:56 -a endtime=2012-02-01T10:04:56\n"
             "\n"
             "#! Get polynomials for bunch of quads using a stdin to give a list of PV names\n"
             "> eget -s names -a pattern=QUAD:LTU1:8%%:POLYCOEF | eget -f -\n"
             "\n"
             , DEFAULT_REQUEST, DEFAULT_TIMEOUT, DEFAULT_PROVIDER);
}



class ChannelGetRequesterImpl : public ChannelGetRequester
{
private:
    string m_channelName;
    bool m_printValue;

    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    Mutex m_pointerMutex;
    Event m_event;

    bool m_done;

public:

    ChannelGetRequesterImpl(std::string channelName, bool printValue) :
        m_channelName(channelName),
        m_printValue(printValue),
        m_done(false)
    {
    }

    virtual string getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    }

    virtual void message(std::string const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status,
                                   ChannelGet::shared_pointer const & channelGet,
                                   epics::pvData::Structure::const_shared_pointer const & /*structure*/)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get create: " << dump_stack_only_on_debug(status) << std::endl;
            }

            channelGet->lastRequest();
            channelGet->get();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel get: " << dump_stack_only_on_debug(status) << std::endl;
            m_event.signal();
        }
    }

    virtual void getDone(const epics::pvData::Status& status,
                         ChannelGet::shared_pointer const & /*channelGet*/,
                         epics::pvData::PVStructure::shared_pointer const & pvStructure,
                         epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get: " << dump_stack_only_on_debug(status) << std::endl;
            }

            // access smart pointers
            {
                Lock lock(m_pointerMutex);
                m_pvStructure = pvStructure;
                m_bitSet = bitSet;
                m_done = true;

                if (m_printValue)
                {
                    printValue(m_channelName, m_pvStructure);
                }
            }
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to get: " << dump_stack_only_on_debug(status) << std::endl;
        }

        m_event.signal();
    }

    PVStructure::shared_pointer getPVStructure()
    {
        Lock lock(m_pointerMutex);
        return m_pvStructure;
    }

    bool waitUntilGet(double timeOut)
    {
        bool signaled = m_event.wait(timeOut);
        if (!signaled)
        {
            std::cerr << "[" << m_channelName << "] get timeout" << std::endl;
            return false;
        }

        Lock lock(m_pointerMutex);
        return m_done;
    }
};


class ChannelRPCRequesterImpl : public ChannelRPCRequester
{
private:
    Mutex m_pointerMutex;
    Event m_event;
    Event m_connectionEvent;
    bool m_successfullyConnected;
    string m_channelName;

    PVStructure::shared_pointer m_lastResponse;
    bool m_done;

public:

    ChannelRPCRequesterImpl(std::string channelName) :
        m_successfullyConnected(false),
        m_channelName(channelName),
        m_done(false)
    {
    }

    virtual string getRequesterName()
    {
        return "ChannelRPCRequesterImpl";
    }

    virtual void message(std::string const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelRPCConnect(const epics::pvData::Status& status, ChannelRPC::shared_pointer const & /*channelRPC*/)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel RPC create: " << dump_stack_only_on_debug(status) << std::endl;
            }

            {
                Lock lock(m_pointerMutex);
                m_successfullyConnected = status.isSuccess();
            }

            m_connectionEvent.signal();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel RPC: " << dump_stack_only_on_debug(status) << std::endl;
            m_connectionEvent.signal();
        }
    }

    virtual void requestDone (const epics::pvData::Status &status,
                              ChannelRPC::shared_pointer const & /*channelRPC*/,
                              epics::pvData::PVStructure::shared_pointer const &pvResponse)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel RPC: " << dump_stack_only_on_debug(status) << std::endl;
            }

            // access smart pointers
            {
                Lock lock(m_pointerMutex);

                m_done = true;
                m_lastResponse = pvResponse;

                /*
                formatNT(std::cout, pvResponse);
                std::cout << std::endl;
                 */
            }
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to RPC: " << dump_stack_only_on_debug(status) << std::endl;
        }

        m_event.signal();
    }

    /*
    void request(epics::pvData::PVStructure::shared_pointer const &pvRequest)
    {
        Lock lock(m_pointerMutex);
        m_channelRPC->request(pvRequest, false);
    }
    */

    PVStructure::shared_pointer getLastResponse()
    {
        Lock lock(m_pointerMutex);
        return m_lastResponse;
    }

    bool waitUntilRPC(double timeOut)
    {
        bool signaled = m_event.wait(timeOut);
        if (!signaled)
        {
            std::cerr << "[" << m_channelName << "] RPC timeout" << std::endl;
            return false;
        }

        Lock lock(m_pointerMutex);
        return m_done;
    }

    bool waitUntilConnected(double timeOut)
    {
        bool signaled = m_connectionEvent.wait(timeOut);
        if (!signaled)
        {
            std::cerr << "[" << m_channelName << "] RPC create timeout" << std::endl;
            return false;
        }

        Lock lock(m_pointerMutex);
        return m_successfullyConnected;
    }
};




class MonitorRequesterImpl : public MonitorRequester
{
private:

    string m_channelName;

public:

    MonitorRequesterImpl(std::string channelName) : m_channelName(channelName) {};

    virtual string getRequesterName()
    {
        return "MonitorRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void monitorConnect(const epics::pvData::Status& status, Monitor::shared_pointer const & monitor, StructureConstPtr const & /*structure*/)
    {
        if (status.isSuccess())
        {
            /*
            string str;
            structure->toString(&str);
            std::cout << str << std::endl;
            */

            Status startStatus = monitor->start();
            // show error
            // TODO and exit
            if (!startStatus.isSuccess())
            {
                std::cerr << "[" << m_channelName << "] channel monitor start: " << startStatus << std::endl;
            }

        }
        else
        {
            std::cerr << "monitorConnect(" << dump_stack_only_on_debug(status) << ")" << std::endl;
        }
    }

    virtual void monitorEvent(Monitor::shared_pointer const & monitor)
    {

        MonitorElement::shared_pointer element;
        while ((element = monitor->poll()))
        {
            if (mode == ValueOnlyMode)
            {
                PVField::shared_pointer value = element->pvStructurePtr->getSubField("value");
                if (value.get() == 0)
                {
                    std::cerr << "no 'value' field" << std::endl;
                    dumpValue(m_channelName, element->pvStructurePtr);
                }
                else
                {
                    Type valueType = value->getField()->getType();
                    if (valueType != scalar && valueType != scalarArray)
                    {
                        // switch to structure mode, unless it's T-type
                        if (valueType == structure && isTType(TR1::static_pointer_cast<PVStructure>(value)))
                        {
                            if (fieldSeparator == ' ')
                                std::cout << std::setw(30) << std::left << m_channelName;
                            else
                                std::cout << m_channelName;
                            std::cout << fieldSeparator;

                            formatTType(std::cout, TR1::static_pointer_cast<PVStructure>(value));
                            std::cout << std::endl;
                        }
                        else
                            dumpValue(m_channelName, element->pvStructurePtr);
                    }
                    else
                    {
                        if (fieldSeparator == ' ')
                            std::cout << std::setw(30) << std::left << m_channelName;
                        else
                            std::cout << m_channelName;
                        std::cout << fieldSeparator;

                        terse(std::cout, value) << std::endl;
                    }
                }
            }
            else if (mode == TerseMode)
            {
                if (fieldSeparator == ' ')
                    std::cout << std::setw(30) << std::left << m_channelName;
                else
                    std::cout << m_channelName;
                std::cout << fieldSeparator;

                terseStructure(std::cout, element->pvStructurePtr) << std::endl;
            }
            else
            {
                dumpValue(m_channelName, element->pvStructurePtr);
            }

            monitor->release(element);
        }

    }

    virtual void unlisten(Monitor::shared_pointer const & /*monitor*/)
    {
        //std::cerr << "unlisten" << std::endl;
        // TODO
        epicsExit(0);
    }
};


/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	eget main()
 * 		Evaluate command line options, set up PVA, connect the
 * 		channels, print the data as requested
 *
 * Arg(s) In:	[options] <pv-name>...
 *
 * Arg(s) Out:	none
 *
 * Return(s):	Standard return code (0=success, 1=error)
 *
 **************************************************************************-*/

int main (int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;
    bool cleanupAndReport = false;

    bool serviceRequest = false;
    bool pvRequestProvidedByUser = false;
    bool onlyQuery = false;

    istream* inputStream = 0;
    ifstream ifs;
    bool fromStream = false;

    string service;
    //string urlEncodedRequest;
    vector< pair<string,string> > parameters;
    bool monitor = false;
    bool quiet = false;
    string defaultProvider = DEFAULT_PROVIDER;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hvr:s:a:w:zNtTmxp:qdcF:f:ni")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'v':               /* Print version */
        {
            Version version("eget", "cpp",
                    EPICS_PVA_MAJOR_VERSION,
                    EPICS_PVA_MINOR_VERSION,
                    EPICS_PVA_MAINTENANCE_VERSION,
                    EPICS_PVA_DEVELOPMENT_FLAG);
            fprintf(stdout, "%s\n", version.getVersionString().c_str());
            return 0;
        }
        case 'w':               /* Set PVA timeout value */
            if((epicsScanDouble(optarg, &timeOut)) != 1 || timeOut <= 0.0)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('eget -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'r':               /* Set pvRequest value */
            request = optarg;
            pvRequestProvidedByUser = true;
            // do not override terse mode
            if (mode == ValueOnlyMode) mode = StructureMode;
            break;
        case 'a':               /* Service parameters */
        {
            string param = optarg;
            size_t eqPos = param.find('=');
            if (eqPos==0)
            {
                // no name

                fprintf(stderr, "Parameter not specified in '-a name=value' or '-a name value' form. ('eget -h' for help.)\n");
                return 1;
            }
            else if (eqPos==string::npos)
            {
                // is next argument actually a value, i.e. "-a name value" form
                if (optind < argc && *argv[optind] != '-')
                {
                    parameters.push_back(pair<string,string>(param, argv[optind]));
                    optind++;
                }
                else
                {
                    // no value

                    //fprintf(stderr, "Parameter not specified in '-a name=value' or '-a name value' form. ('eget -h' for help.)\n");
                    //return 1;
                    parameters.push_back(pair<string,string>(param, ""));
                }
            }
            else
            {
                parameters.push_back(pair<string,string>(param.substr(0, eqPos), param.substr(eqPos+1, string::npos)));
            }
            /*
            if (urlEncodedRequest.size())
                urlEncodedRequest += '&';
            char* encoded = url_encode(optarg);
            urlEncodedRequest += encoded;
            free(encoded);
            */
            break;
        }
        case 's':               /* Service name */
            service = optarg;
            serviceRequest = true;
            break;
        case 'z':               /* pvAccess RPC mode */
            onlyQuery = true;
            break;
        case 'N':               /* Do not format NT types */
            dumpStructure = true;
            break;
        case 'i':               /* T-types format mode */
            formatTTypes(false);
            break;
        case 't':               /* Terse mode */
            mode = TerseMode;
            break;
        case 'T':               /* Terse mode */
            transpose = true;
            break;
        case 'm':               /* Monitor mode */
            monitor = true;
            break;
        case 'x':               /* Column-major order mode */
            columnMajor = true;
            break;
        case 'p':               /* Provider name */
            defaultProvider = optarg;

            // for now no only pva/ca schema is supported
            // TODO
            if (defaultProvider != "pva" && defaultProvider != "ca")
            {
                std::cerr << "invalid default provider '" << defaultProvider << "', only 'pva' and 'ca' are supported" << std::endl;
                // TODO
                return 1;
            }

            break;
        case 'q':               /* Quiet mode */
            quiet = true;
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'c':               /* Clean-up and report used instance count */
            cleanupAndReport = true;
            break;
        case 'F':               /* Store this for output formatting */
            fieldSeparator = (char) *optarg;
            break;
        case 'f':               /* Use input stream as input */
        {
            string fileName = optarg;
            if (fileName == "-")
                inputStream = &cin;
            else
            {
                ifs.open(fileName.c_str(), ifstream::in);
                if (!ifs)
                {
                    fprintf(stderr,
                            "Failed to open file '%s'.\n",
                            fileName.c_str());
                    return 1;
                }
                else
                    inputStream = &ifs;
            }

            fromStream = true;
            break;
        }
        case 'n':
            setEnumPrintMode(NumberEnum);
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('eget -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('eget -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    int nPvs = argc - optind;       /* Remaining arg list are PV names */
    if (nPvs > 0)
    {
        // do not allow reading file and command line specified pvs
        fromStream = false;
    }
    else if (nPvs < 1 && !serviceRequest && !fromStream)
    {
        fprintf(stderr, "No PV name(s) specified. ('eget -h' for help.)\n");
        return 1;
    }

    // only one pv, arguments provided without serviceRequest switch
    if (nPvs == 1 && parameters.size() > 0)
    {
        // switch to serviceRequest
        service = argv[optind];
        serviceRequest = true;
        nPvs = 0;
    }
    else if (nPvs > 0 && serviceRequest)
    {
        fprintf(stderr, "PV name(s) specified and service query requested. ('eget -h' for help.)\n");
        return 1;
    }

    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    std::cout << std::boolalpha;
    terseSeparator(fieldSeparator);
    terseArrayCount(false);

    bool allOK = true;

    Requester::shared_pointer requester(new RequesterImpl("eget"));

    // parse URI
    // try to parse as URI if only one nPvs
    URI uri;
    bool validURI =
        (serviceRequest || nPvs == 1) ?
        URI::parse(serviceRequest ? service : argv[optind], uri) :
        false;

    // if there is only one nPvs and it's a valid URI that has ? character,
    // then it's an service (RPC) request
    if (!serviceRequest && validURI && uri.query_indicated)
    {
        service = argv[optind];
        serviceRequest = true;
    }

    static string noAddress;

    // PVs mode
    if (!serviceRequest)
    {
        vector<string> pvs;
        vector<string> pvsAddress;
        vector<string> providerNames;

        if (validURI)
        {
            // standard get request
            // for now no only pva/ca schema is supported, without authority
            // TODO
            if (uri.protocol != "pva" && uri.protocol != "ca")
            {
                std::cerr << "invalid URI scheme '" << uri.protocol << "', only 'pva' and 'ca' are supported" << std::endl;
                // TODO
                return 1;
            }

            if (uri.path.length() <= 1)
            {
                std::cerr << "invalid URI, empty path" << std::endl;
                // TODO
                return 1;
            }

            // skip trailing '/'
            pvs.push_back(uri.path.substr(1));
            pvsAddress.push_back(uri.host);
            providerNames.push_back(uri.protocol);
        }
        else
        {
            for (int n = 0; optind < argc; n++, optind++)
            {
                URI uri;
                bool validURI = URI::parse(argv[optind], uri);
                if (validURI)
                {
                    // TODO this is copy&pase code from above, clean it up
                    // for now no only pva/ca schema is supported, without authority
                    // TODO
                    if (uri.protocol != "pva" && uri.protocol != "ca")
                    {
                        std::cerr << "invalid URI scheme '" << uri.protocol << "', only 'pva' and 'ca' are supported" << std::endl;
                        // TODO
                        return 1;
                    }

                    if (uri.path.length() <= 1)
                    {
                        std::cerr << "invalid URI, empty path" << std::endl;
                        // TODO
                        return 1;
                    }

                    // skip trailing '/'
                    pvs.push_back(uri.path.substr(1));
                    pvsAddress.push_back(uri.host);
                    providerNames.push_back(uri.protocol);
                }
                else
                {
                    pvs.push_back(argv[optind]);
                    pvsAddress.push_back(noAddress);
                    providerNames.push_back(defaultProvider);
                }
            }
        }

        PVStructure::shared_pointer pvRequest =
            CreateRequest::create()->createRequest(request);
        if(pvRequest.get()==0) {
            fprintf(stderr, "failed to parse request string\n");
            return 1;
        }

        // register "pva" and "ca" providers
        ClientFactory::start();
        epics::pvAccess::ca::CAClientFactory::start();

        // first connect to all, this allows resource (e.g. TCP connection) sharing
        vector<Channel::shared_pointer> channels(nPvs);
        for (int n = 0; n < nPvs; n++)
        {
            TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl(quiet));
            if (pvsAddress[n].empty())
                channels[n] = getChannelProviderRegistry()->getProvider(providerNames[n])->createChannel(pvs[n], channelRequesterImpl);
            else
                channels[n] = getChannelProviderRegistry()->getProvider(providerNames[n])->createChannel(pvs[n], channelRequesterImpl,
                              ChannelProvider::PRIORITY_DEFAULT, pvsAddress[n]);
        }

        // TODO maybe unify for nPvs == 1?!
        // we cannot collect when fromStream is true, since we want to print value immediately
        bool collectValues = (mode == ValueOnlyMode) && nPvs > 1 && !fromStream;

        vector<PVStructure::shared_pointer> collectedValues;
        shared_vector<string> collectedNames;
        if (collectValues)
        {
            collectedValues.reserve(nPvs);
            collectedNames.reserve(nPvs);
        }

        // for now a simple iterating sync implementation, guarantees order
        int n = -1;
        while (true)
        {
            Channel::shared_pointer channel;

            if (!fromStream)
            {
                if (++n >= nPvs)
                    break;
                channel = channels[n];
            }
            else
            {
                string cn;
                string ca;
                string cp;

                // read next channel name from stream
                *inputStream >> cn;
                if (!(*inputStream))
                    break;

                URI uri;
                bool validURI = URI::parse(cn.c_str(), uri);
                if (validURI)
                {
                    // TODO this is copy&pase code from above, clean it up
                    // for now no only pva/ca schema is supported, without authority
                    // TODO
                    if (uri.protocol != "pva" && uri.protocol != "ca")
                    {
                        std::cerr << "invalid URI scheme '" << uri.protocol << "', only 'pva' and 'ca' are supported" << std::endl;
                        // TODO
                        return 1;
                    }

                    if (uri.path.length() <= 1)
                    {
                        std::cerr << "invalid URI, empty path" << std::endl;
                        // TODO
                        return 1;
                    }

                    // skip trailing '/'
                    cn = uri.path.substr(1);
                    ca = uri.host;
                    cp = uri.protocol;
                }
                else
                {
                    // leave cn as it is, use default provider
                    ca = noAddress;
                    cp = defaultProvider;
                }



                TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl(quiet));
                if (ca.empty())
                    channel = getChannelProviderRegistry()->getProvider(cp)->createChannel(cn, channelRequesterImpl);
                else
                    channel = getChannelProviderRegistry()->getProvider(cp)->createChannel(cn, channelRequesterImpl,
                              ChannelProvider::PRIORITY_DEFAULT, ca);
            }

            if (monitor)
            {
                TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl = TR1::dynamic_pointer_cast<ChannelRequesterImpl>(channel->getChannelRequester());
                channelRequesterImpl->showDisconnectMessage();

                // TODO remove this line, when CA provider will allow creation of monitors
                // when channels is yet not connected
                if (channelRequesterImpl->waitUntilConnected(timeOut))
                {
                    TR1::shared_ptr<MonitorRequesterImpl> monitorRequesterImpl(new MonitorRequesterImpl(channel->getChannelName()));
                    channel->createMonitor(monitorRequesterImpl, pvRequest);
                }
                else
                {
                    allOK = false;
                    channel->destroy();
                    std::cerr << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
                }
            }
            else
            {
                /*
                TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl());
                Channel::shared_pointer channel = provider->createChannel(pvs[n], channelRequesterImpl);
                */

                TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl = TR1::dynamic_pointer_cast<ChannelRequesterImpl>(channel->getChannelRequester());

                if (channelRequesterImpl->waitUntilConnected(timeOut))
                {
                    TR1::shared_ptr<GetFieldRequesterImpl> getFieldRequesterImpl;

                    // probe for value field
                    // but only if there is only one PV request (otherwise mode change makes a mess)
                    if (mode == ValueOnlyMode && nPvs == 1)
                    {
                        getFieldRequesterImpl.reset(new GetFieldRequesterImpl(channel));
                        // get all to be immune to bad clients not supporting selective getField request
                        channel->getField(getFieldRequesterImpl, "");
                    }

                    if (getFieldRequesterImpl.get() == 0 ||
                            getFieldRequesterImpl->waitUntilFieldGet(timeOut))
                    {
                        // check probe
                        if (getFieldRequesterImpl.get())
                        {
                            Structure::const_shared_pointer structure =
                                TR1::dynamic_pointer_cast<const Structure>(getFieldRequesterImpl->getField());
                            if (structure.get() == 0 || structure->getField("value").get() == 0)
                            {
                                // fallback to structure
                                mode = StructureMode;
                                pvRequest = CreateRequest::create()->createRequest("field()");
                            }
                        }

                        TR1::shared_ptr<ChannelGetRequesterImpl> getRequesterImpl(
                            new ChannelGetRequesterImpl(channel->getChannelName(), false)
                        );
                        ChannelGet::shared_pointer channelGet = channel->createChannelGet(getRequesterImpl, pvRequest);
                        bool ok = getRequesterImpl->waitUntilGet(timeOut);
                        allOK &= ok;
                        if (ok)
                        {
                            if (collectValues)
                            {
                                collectedValues.push_back(getRequesterImpl->getPVStructure());
                                collectedNames.push_back(channel->getChannelName());
                            }
                            else
                            {
                                // print immediately
                                printValue(channel->getChannelName(), getRequesterImpl->getPVStructure(), fromStream);
                            }
                        }
                    }
                    else
                    {
                        allOK = false;
                        channel->destroy();
                        std::cerr << "[" << channel->getChannelName() << "] failed to get channel introspection data" << std::endl;
                    }
                }
                else
                {
                    allOK = false;
                    channel->destroy();
                    std::cerr << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
                }
            }
        }

        if (collectValues)
            printValues(freeze(collectedNames), collectedValues);

        if (allOK && monitor)
        {
            while (true)
                epicsThreadSleep(timeOut);
        }

        epics::pvAccess::ca::CAClientFactory::stop();
        ClientFactory::stop();
    }
    // service RPC mode
    else
    {
        string authority;

        if (validURI)
        {
            if (uri.protocol != "pva")
            {
                std::cerr << "invalid URI scheme '" << uri.protocol << "', only 'pva' is supported" << std::endl;
                // TODO
                return 1;
            }

            authority = uri.host;

            if (uri.path.length() <= 1)
            {
                std::cerr << "invalid URI, empty path" << std::endl;
                // TODO
                return 1;
            }

            // skip trailing '/'
            service = uri.path.substr(1);

            string::const_iterator end_i = uri.query.end();
            string::const_iterator begin_i = uri.query.begin();
            while (begin_i != end_i)
            {
                string::const_iterator pair_end_i = find(begin_i, end_i, '&');

                string::const_iterator name_end_i = find(begin_i, pair_end_i, '=');
                if (name_end_i != pair_end_i)
                {
                    string name(begin_i, name_end_i);
                    string value(name_end_i+1, pair_end_i);
                    parameters.push_back(pair<string,string>(name, value));
                }
                else
                {
                    //fprintf(stderr, "Parameter not specified in name=value form. ('eget -h' for help.)\n");
                    //return 1;
                    string name(begin_i, pair_end_i);
                    parameters.push_back(pair<string,string>(name, ""));
                }

                begin_i = pair_end_i;
                if (begin_i != end_i)
                    begin_i++;	// skip '&'
            }
        }

        /*
        std::cerr << "service            : " << service << std::endl;
        std::cerr << "parameters         : " << std::endl;

        vector< pair<string, string> >::iterator iter = parameters.begin();
        for (; iter != parameters.end(); iter++)
            std::cerr << "    " << iter->first << " = " << iter->second << std::endl;
        //std::cerr << "encoded URL request: '" << urlEncodedRequest << "'" << std::endl;
        */

        // simply empty
        PVStructure::shared_pointer pvRequest =
            CreateRequest::create()->createRequest(
                !pvRequestProvidedByUser ? DEFAULT_RPC_REQUEST : request
            );
        if(pvRequest.get()==NULL) {
            fprintf(stderr, "failed to parse request string\n");
            return 1;
        }


        StringArray queryFieldNames;
        FieldConstPtrArray queryFields;
        for (vector< pair<string, string> >::iterator iter = parameters.begin();
                iter != parameters.end();
                iter++)
        {
            queryFieldNames.push_back(iter->first);
            queryFields.push_back(getFieldCreate()->createScalar(pvString));
        }

        Structure::const_shared_pointer queryStructure(
            getFieldCreate()->createStructure(
                queryFieldNames,
                queryFields
            )
        );



        StringArray uriFieldNames;
        uriFieldNames.push_back("scheme");
        if (!authority.empty()) uriFieldNames.push_back("authority");
        uriFieldNames.push_back("path");
        uriFieldNames.push_back("query");

        FieldConstPtrArray uriFields;
        uriFields.push_back(getFieldCreate()->createScalar(pvString));
        if (!authority.empty()) uriFields.push_back(getFieldCreate()->createScalar(pvString));
        uriFields.push_back(getFieldCreate()->createScalar(pvString));
        uriFields.push_back(queryStructure);

        Structure::const_shared_pointer uriStructure(
            getFieldCreate()->createStructure(
                "epics:nt/NTURI:1.0",
                uriFieldNames,
                uriFields
            )
        );



        PVStructure::shared_pointer request(
            getPVDataCreate()->createPVStructure(uriStructure)
        );

        request->getSubField<PVString>("scheme")->put("pva");
        if (!authority.empty()) request->getSubField<PVString>("authority")->put(authority);
        request->getSubField<PVString>("path")->put(service);
        PVStructure::shared_pointer query = request->getSubField<PVStructure>("query");
        for (vector< pair<string, string> >::iterator iter = parameters.begin();
                iter != parameters.end();
                iter++)
        {
            query->getSubField<PVString>(iter->first)->put(iter->second);
        }


        PVStructure::shared_pointer arg = onlyQuery ? query : request;
        if (debug)
        {
            std::cout << "Request structure: " << std::endl << *(arg.get()) << std::endl;
        }


        ClientFactory::start();
        ChannelProvider::shared_pointer provider = getChannelProviderRegistry()->getProvider("pva");

        TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl(quiet));
        Channel::shared_pointer channel =
            authority.empty() ?
            provider->createChannel(service, channelRequesterImpl) :
            provider->createChannel(service, channelRequesterImpl,
                                    ChannelProvider::PRIORITY_DEFAULT, authority);

        if (channelRequesterImpl->waitUntilConnected(timeOut))
        {
            TR1::shared_ptr<ChannelRPCRequesterImpl> rpcRequesterImpl(new ChannelRPCRequesterImpl(channel->getChannelName()));
            ChannelRPC::shared_pointer channelRPC = channel->createChannelRPC(rpcRequesterImpl, pvRequest);

            if (rpcRequesterImpl->waitUntilConnected(timeOut))
            {
                channelRPC->lastRequest();
                channelRPC->request(arg);
                allOK &= rpcRequesterImpl->waitUntilRPC(timeOut);
                if (allOK)
                {
                    if (dumpStructure)
                    {
                        if (rpcRequesterImpl->getLastResponse().get() == 0)
                            std::cout << "(null)" << std::endl;
                        else
                        {
                            //std::cout << *(rpcRequesterImpl->getLastResponse().get()) << std::endl;
                            pvutil_ostream myos(std::cout.rdbuf());
                            myos << *(rpcRequesterImpl->getLastResponse().get()) << std::endl;
                        }
                    }
                    else
                        formatNT(std::cout, rpcRequesterImpl->getLastResponse());
                    std::cout << std::endl;
                }
            }
            else
            {
                allOK = false;
            }
        }
        else
        {
            allOK = false;
            std::cerr << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
        }

        channel->destroy();

        ClientFactory::stop();
    }

    if (cleanupAndReport)
    {
        // TODO implement wait on context
        epicsThreadSleep ( 3.0 );
        //std::cerr << "-----------------------------------------------------------------------" << std::endl;
        //epicsExitCallAtExits();
    }

    return allOK ? 0 : 1;
}

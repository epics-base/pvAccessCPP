#include <iostream>
#include <pv/clientFactory.h>
#include <pv/pvAccess.h>

#include <caProvider.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsThread.h>
#include <pv/logger.h>
#include <pv/lock.h>

#include <vector>
#include <string>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <map>

#include <pv/convert.h>
#include <pv/event.h>
#include <epicsExit.h>

#include "pvutils.cpp"

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

enum PrintMode { ValueOnlyMode, StructureMode, TerseMode };
PrintMode mode = ValueOnlyMode;

char fieldSeparator = ' ';

bool columnMajor = false;

bool transpose = false;

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
    PVScalarPtr value = dynamic_pointer_cast<PVScalar>(pvStruct->getSubField("value"));
    if (value.get() == 0)
    {
        std::cerr << "no scalar_t 'value' field in NTScalar" << std::endl;
        return;
    }

    o << *value;
}

std::ostream& formatVector(std::ostream& o,
                           String label,
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
    PVScalarArrayPtr value = dynamic_pointer_cast<PVScalarArray>(pvStruct->getSubField("value"));
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
    PVStructurePtr enumt = dynamic_pointer_cast<PVStructure>(pvStruct->getSubField("value"));
    if (enumt.get() == 0)
    {
        std::cerr << "no enum_t 'value' field in NTEnum" << std::endl;
        return;
    }

    PVIntPtr index = dynamic_pointer_cast<PVInt>(enumt->getSubField("index"));
    if (index.get() == 0)
    {
        std::cerr << "no int 'value.index' field in NTEnum" << std::endl;
        return;
    }

    PVStringArrayPtr choices = dynamic_pointer_cast<PVStringArray>(enumt->getSubField("choices"));
    if (choices.get() == 0)
    {
        std::cerr << "no string[] 'value.choices' field in NTEnum" << std::endl;
        return;
    }
    
    int32 ix = index->get();
    if (ix < 0 || ix > static_cast<int32>(choices->getLength()))
    {
        o << ix;
    }
    else
    {
        choices->dumpValue(o, ix);
    }
}

size_t getLongestString(vector<String> const & array)
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
                 vector<String> const & labels,
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
                if (separator == ' ')
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
                if (separator == ' ')
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
    PVStringArrayPtr labels = dynamic_pointer_cast<PVStringArray>(pvStruct->getScalarArrayField("labels", pvString));
    if (labels.get() == 0)
    {
        std::cerr << "no string[] 'labels' field in NTTable" << std::endl;
        return;
    }

    PVStructurePtr value = pvStruct->getStructureField("value");
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
        PVScalarArrayPtr array = dynamic_pointer_cast<PVScalarArray>(fields[i]);
        if (array.get() == 0)
        {
            std::cerr << "malformed NTTable, " << (i+1) << ". field is not scalar_t[]" << std::endl;
            return;
        }

        columnData.push_back(array);
    }

    // get labels
    StringArrayData labelsData;
    labels->get(0, numColumns, labelsData);

    bool showHeader = (mode != TerseMode);
    formatTable(o, labelsData.data, columnData, showHeader, transpose);
}    


void formatNTMatrix(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVDoubleArrayPtr value = dynamic_pointer_cast<PVDoubleArray>(pvStruct->getScalarArrayField("value", pvDouble));
    if (value.get() == 0)
    {
        std::cerr << "no double[] 'value' field in NTMatrix" << std::endl;
        return;
    }

    int32 rows, cols;

    PVIntArrayPtr dim = dynamic_pointer_cast<PVIntArray>(pvStruct->getScalarArrayField("dim", pvInt));
    if (dim.get() != 0)
    {
        // dim[] = { rows, columns }
        size_t dims = dim->getLength();
        if (dims != 1 && dims != 2)
        {
            std::cerr << "malformed NTMatrix, dim[] must contain 1 or 2 elements instead of  " << dims << std::endl;
            return;
        }

        IntArrayData data;
        dim->get(0, dims, data);
        rows = data.data[0];
        cols = (dims == 2) ? data.data[1] : 1;

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
                if (separator == ' ')
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
                if (separator == ' ')
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


void formatNTNameValue(std::ostream& o, PVStructurePtr const & pvStruct)
{
    PVStringArrayPtr name = dynamic_pointer_cast<PVStringArray>(pvStruct->getScalarArrayField("name", pvString));
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

    PVScalarArrayPtr array = dynamic_pointer_cast<PVScalarArray>(value);
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
    StringArrayData nameData;
    name->get(0, name->getLength(), nameData);

    // get max column name size
    bool showHeader = (mode != TerseMode);
    size_t maxLabelColumnLength = showHeader ? getLongestString(nameData.data) : 0;

    size_t maxColumnLength = getLongestString(array);

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
                    int width = std::max(nameData.data[i].size()+padding, maxColumnLength);
                    o << std::setw(width) << std::right;
                    // non-compact o << std::setw(maxColumnLength) << std::right;
                }
                else if (i > 0)
                {
                    o << separator;
                }

                o << nameData.data[i];
            }
            o << std::endl;
        }

        // then values
        for (size_t i = 0; i < numColumns; i++)
        {
            if (separator == ' ')
            {
                int width = std::max(nameData.data[i].size()+padding, maxColumnLength);
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
                o << nameData.data[i];
            }

            if (separator == ' ')
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
    PVStringPtr scheme = dynamic_pointer_cast<PVString>(pvStruct->getStringField("scheme"));
    if (scheme.get() == 0)
    {
        std::cerr << "no string 'scheme' field in NTURI" << std::endl;
    }

    PVStringPtr authority = dynamic_pointer_cast<PVString>(pvStruct->getSubField("authority"));

    PVStringPtr path = dynamic_pointer_cast<PVString>(pvStruct->getStringField("path"));
    if (path.get() == 0)
    {
        std::cerr << "no string 'path' field in NTURI" << std::endl;
        return;
    }

    PVStructurePtr query = dynamic_pointer_cast<PVStructure>(pvStruct->getSubField("query"));

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


void formatNTImage(std::ostream& /*o*/, PVStructurePtr const & pvStruct)
{
    PVIntPtr colorMode = pvStruct->getIntField("colorMode");
    if (colorMode.get() == 0)
    {
        std::cerr << "no int 'colorMode' field in NTImage" << std::endl;
        return;
    }
    int32 cm = colorMode->get();
    if (cm != 0 && cm != 1 && cm != 2)
    {
        std::cerr << "unsupported image 'colorMode', only {0,1,2} modes are supported" << std::endl;
        return;
    }

    PVScalarArrayPtr value = dynamic_pointer_cast<PVScalarArray>(pvStruct->getSubField("value"));
    if (value.get() == 0)
    {
        std::cerr << "no scalar array 'value' field in NTImage" << std::endl;
        return;
    }

    int32 rows, cols;

    PVIntArrayPtr dim = dynamic_pointer_cast<PVIntArray>(pvStruct->getScalarArrayField("dim", pvInt));
    if (dim.get() == 0)
    {
        std::cerr << "no int[] 'dim' field in NTImage" << std::endl;
        return;
    }
    
    // dim[] = { rows, columns } or
    // dim[] = { 3, rows, columns }
    IntArrayData data;
    size_t dims = dim->getLength();
    dim->get(0, dims, data);
    size_t imageSize;
    if ((cm == 0 || cm == 1) && dims == 2)
    {
        cols = data.data[0];
        rows = data.data[1];
        imageSize = cols * rows;
    }
    else if (cm == 2 && dims == 3)
    {
        cols = data.data[1];
        rows = data.data[2];
        imageSize = cols * rows * 3;
    }
    else
    {
        std::cerr << "malformed NTImage, dim[] is invalid for specified color mode" << std::endl;
        return;
    }

    if (rows <= 0 || cols <= 0)
    {
        std::cerr << "malformed NTImage, dim[] must contain elements > 0" << std::endl;
        return;
    }

    PVByteArrayPtr array = dynamic_pointer_cast<PVByteArray>(value);
    if (array.get() == 0)
    {
        std::cerr << "currently only byte[] value field is supported" << std::endl;
        return;
    }

    if (array->getLength() != imageSize)
    {
        std::cerr << "byte[] length does not match expected image size (" <<
                     array->getLength() << " != " << imageSize << ")" << std::endl;
        return;
    }

    ByteArrayData img;
    array->get(0, array->getLength(), img);
    /*
    size_t len = static_cast<size_t>(rows*cols);
    for (size_t i = 0; i < len; i++)
        o << img.data[i];
    */
    //eget -s testImage | gnuplot  -e "set size ratio -1; set palette grey; set cbrange [0:255]; plot '-'  binary array=(512,384) flipy format='%uchar' with image"

    FILE* gnuplotPipe = popen ("gnuplot -p", "w");

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
            fprintf(gnuplotPipe, "%c", img.data[i]);
    }
    else
    {
        // grayscale

        fprintf(gnuplotPipe, "set palette grey\n");
        fprintf(gnuplotPipe, "set cbrange [0:255]\n");

        fprintf(gnuplotPipe, "plot '-'  binary array=(%u,%u) flipy format='%%uchar' with image\n", cols, rows);

        for (size_t i = 0; i < imageSize; i++)
            fprintf(gnuplotPipe, "%c", img.data[i]);
    }

    fflush(gnuplotPipe);
    pclose(gnuplotPipe);

}

typedef void(*NTFormatterFunc)(std::ostream& o, PVStructurePtr const & pvStruct);
typedef map<String, NTFormatterFunc> NTFormatterLUTMap;
NTFormatterLUTMap ntFormatterLUT;

void initializeNTFormatterLUT()
{
    ntFormatterLUT["uri:ev4:nt/2012/pwd:NTScalar"] = formatNTScalar;
    ntFormatterLUT["uri:ev4:nt/2012/pwd:NTScalarArray"] = formatNTScalarArray;
    ntFormatterLUT["uri:ev4:nt/2012/pwd:NTEnum"] = formatNTEnum;
    ntFormatterLUT["uri:ev4:nt/2012/pwd:NTTable"] = formatNTTable;
    ntFormatterLUT["uri:ev4:nt/2012/pwd:NTMatrix"] = formatNTMatrix;
    ntFormatterLUT["uri:ev4:nt/2012/pwd:NTAny"] = formatNTAny;
    ntFormatterLUT["uri:ev4:nt/2012/pwd:NTNameValue"] = formatNTNameValue;
    ntFormatterLUT["uri:ev4:nt/2012/pwd:NTURI"] = formatNTURI;
    ntFormatterLUT["uri:ev4:nt/2012/pwd:NTImage"] = formatNTImage;
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
        PVStructurePtr pvStruct = static_pointer_cast<PVStructure>(pv);
        {
            String id = pvStruct->getField()->getID();

            NTFormatterLUTMap::const_iterator formatter = ntFormatterLUT.find(id);
            if (formatter != ntFormatterLUT.end())
            {
                (formatter->second)(o, pvStruct);
            }
            else
            {
                std::cerr << "unsupported normative type" << std::endl;
                o << *(pv.get()) << std::endl;
            }

            return;
        }
    }
    
    // no ID, just dump
    o << *(pv.get()) << std::endl;
}

void dumpValue(String const & channelName, PVField::shared_pointer const & pv)
{
    if (!channelName.empty())
        std::cout << channelName << std::endl;
    std::cout << *(pv.get()) << std::endl << std::endl;
}

void printValue(String const & channelName, PVStructure::shared_pointer const & pv, bool forceTerseWithName = false)
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
                //formatScalarArray(std::cout, dynamic_pointer_cast<PVScalarArray>(value));
                formatVector(std::cout, "", dynamic_pointer_cast<PVScalarArray>(value), false);
            }
            else
            {
                // switch to structure mode, unless it's NTEnum
                if (value->getField()->getID() == "enum_t")
                {
                    formatNTEnum(std::cout, pv);
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

static String emptyString;

// only in ValueOnlyMode
// NOTE: names might be empty
void printValues(vector<string> const & names, vector<PVStructure::shared_pointer> const & values)
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
                scalarArrays.push_back(dynamic_pointer_cast<PVScalarArray>(value));
            else if (type == scalar)
            {
                PVScalar::shared_pointer scalar = dynamic_pointer_cast<PVScalar>(value);
                scalars.push_back(scalar);

                // make an array, i.e. PVStringArray, out of a scalar (since scalar is an array w/ element count == 1)
                PVStringArray::shared_pointer stringArray =
                        dynamic_pointer_cast<PVStringArray>(getPVDataCreate()->createPVScalarArray(pvString));
                StringArray values;
                values.reserve(1);
                values.push_back(getConvert()->toString(scalar));
                stringArray->put(0, values.size(), values, 0);
                scalarArrays.push_back(stringArray);
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

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);


void usage (void)
{
    fprintf (stderr, "\nUsage: eget [options] [<PV name>... | -s <service name>]\n\n"
             "  -h: Help: Print this message\n"
             "\noptions:\n"
             "  -s <service name>:   Service API compliant based RPC service name (accepts NTURI request argument)\n"
             "  -a <service arg>:    Service argument in form 'name[=value]'\n"
             "  -r <pv request>:     Get request string, specifies what fields to return and options, default is '%s'\n"
             "  -w <sec>:            Wait time, specifies timeout, default is %f second(s)\n"
             "  -z:                  Pure pvAccess RPC based service (send NTURI.query as request argument)\n"
             "  -n:                  Do not format NT types, dump structure instead.\n"
             "  -t:                  Terse mode.\n"
             "  -T:                  Transpose vector, table, matrix.\n"
             "  -x:                  Use column-major order to decode matrix.\n"
             "  -q:                  Quiet mode, print only error messages\n"
             "  -d:                  Enable debug output\n"
             "  -F <ofs>:            Use <ofs> as an alternate output field separator\n"
             "  -c:                  Wait for clean shutdown and report used instance count (for expert users)"
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
             , DEFAULT_REQUEST, DEFAULT_TIMEOUT);
}


class ChannelGetRequesterImpl : public ChannelGetRequester
{
private:
    String m_channelName;
    bool m_printValue;

    ChannelGet::shared_pointer m_channelGet;
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    Mutex m_pointerMutex;
    Event m_event;

    bool m_done;

public:
    
    ChannelGetRequesterImpl(String channelName, bool printValue) :
        m_channelName(channelName),
        m_printValue(printValue),
        m_done(false)
    {
    }
    
    virtual String getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    }

    virtual void message(String const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status,ChannelGet::shared_pointer const & channelGet,
                                   epics::pvData::PVStructure::shared_pointer const & pvStructure,
                                   epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get create: " << status << std::endl;
            }
            
            // assign smart pointers
            {
                Lock lock(m_pointerMutex);
                m_channelGet = channelGet;
                m_pvStructure = pvStructure;
                m_bitSet = bitSet;
            }
            
            channelGet->get(true);
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel get: " << status << std::endl;
            m_event.signal();
        }
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get: " << status << std::endl;
            }

            // access smart pointers
            {
                Lock lock(m_pointerMutex);
                {
                    m_done = true;

                    if (m_printValue)
                    {
                        // needed since we access the data
                        ScopedLock dataLock(m_channelGet);

                        printValue(m_channelName, m_pvStructure);
                    }
                }

                // this is OK since callee holds also owns it
                m_channelGet.reset();
            }
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to get: " << status << std::endl;
            {
                Lock lock(m_pointerMutex);
                // this is OK since caller holds also owns it
                m_channelGet.reset();
            }
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
    ChannelRPC::shared_pointer m_channelRPC;
    Mutex m_pointerMutex;
    Event m_event;
    Event m_connectionEvent;
    String m_channelName;

    PVStructure::shared_pointer m_lastResponse;
    bool m_done;

public:
    
    ChannelRPCRequesterImpl(String channelName) : m_channelName(channelName), m_done(false) {}
    
    virtual String getRequesterName()
    {
        return "ChannelRPCRequesterImpl";
    }

    virtual void message(String const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelRPCConnect(const epics::pvData::Status& status,ChannelRPC::shared_pointer const & channelRPC)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel RPC create: " << status << std::endl;
            }
            
            // assign smart pointers
            {
                Lock lock(m_pointerMutex);
                m_channelRPC = channelRPC;
            }
            
            m_connectionEvent.signal();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel get: " << status << std::endl;
            m_connectionEvent.signal();
        }
    }

    virtual void requestDone (const epics::pvData::Status &status, epics::pvData::PVStructure::shared_pointer const &pvResponse)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel RPC: " << status << std::endl;
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

                // this is OK since calle holds also owns it
                m_channelRPC.reset();
            }
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to RPC: " << status << std::endl;
            {
                Lock lock(m_pointerMutex);
                // this is OK since caller holds also owns it
                m_channelRPC.reset();
            }
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

        bool connected;
        {
            Lock lock(m_pointerMutex);
            connected = (m_channelRPC.get() != 0);
        }
        return connected ? true : false;
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
    bool onlyQuery = false;
    bool dumpStructure = false;
    string service;
    //string urlEncodedRequest;
    vector< pair<string,string> > parameters;
    bool quiet = false;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":hr:s:a:w:zntTxqdcF:")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'w':               /* Set PVA timeout value */
            if(epicsScanDouble(optarg, &timeOut) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('eget -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'r':               /* Set pvRequest value */
            request = optarg;
            // do not override terse mode
            if (mode == ValueOnlyMode) mode = StructureMode;
            break;
        case 'a':               /* Service parameters */
        {
            string param = optarg;
            size_t eqPos = param.find('=');
            if (eqPos==string::npos)
            {
                //fprintf(stderr, "Parameter not specified in name=value form. ('eget -h' for help.)\n");
                //return 1;
                parameters.push_back(pair<string,string>(param, ""));
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
        case 'n':               /* Do not format NT types */
            dumpStructure = true;
            break;
        case 't':               /* Terse mode */
            mode = TerseMode;
            break;
        case 'T':               /* Terse mode */
            transpose = true;
            break;
        case 'x':               /* Column-major order mode */
            columnMajor = true;
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
    if (nPvs < 1 && !serviceRequest)
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
    if (validURI && uri.query_indicated)
    {
        service = argv[optind];
        serviceRequest = true;
    }

    // PVs mode
    if (!serviceRequest)
    {
        vector<string> pvs;
        vector<string> providerNames;

        if (validURI)
        {
            // standard get request
            // for now no only pva/ca schema is supported, without authority
            // TODO
            if (uri.protocol != "pva" && uri.protocol != "ca")
            {
                std::cerr << "invalid URI scheme '" << uri.protocol << "', only 'pva' and 'ca' is supported" << std::endl;
                // TODO
                return 1;
            }

            // authority = uri.host;

            if (uri.path.length() <= 1)
            {
                std::cerr << "invalid URI, empty path" << std::endl;
                // TODO
                return 1;
            }

            // skip trailing '/'
            pvs.push_back(uri.path.substr(1));
            providerNames.push_back(uri.protocol);
        }
        else
        {
            // TODO URI support
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
                        std::cerr << "invalid URI scheme '" << uri.protocol << "', only 'pva' and 'ca' is supported" << std::endl;
                        // TODO
                        return 1;
                    }

                    // authority = uri.host;

                    if (uri.path.length() <= 1)
                    {
                        std::cerr << "invalid URI, empty path" << std::endl;
                        // TODO
                        return 1;
                    }

                    // skip trailing '/'
                    pvs.push_back(uri.path.substr(1));
                    providerNames.push_back(uri.protocol);
                }
                else
                {
                    // defaults to "pva"
                    pvs.push_back(argv[optind]);
                    providerNames.push_back("pva");
                }
            }
        }

        PVStructure::shared_pointer pvRequest =
                getCreateRequest()->createRequest(request, requester);
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
            shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl(quiet));

            // TODO to be removed
            String providerName = providerNames[n];
            if (providerName == "pva")
                providerName = "pvAccess";

            // TODO no privder check
            channels[n] = getChannelAccess()->getProvider(providerName)->createChannel(pvs[n], channelRequesterImpl);
        }

        // TODO maybe unify for nPvs == 1?!
        bool collectValues = (mode == ValueOnlyMode) && nPvs > 1;

        vector<PVStructure::shared_pointer> collectedValues;
        collectedValues.reserve(nPvs);
        vector<String> collectedNames;
        collectedNames.reserve(nPvs);

        // for now a simple iterating sync implementation, guarantees order
        for (int n = 0; n < nPvs; n++)
        {
            /*
            shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl());
            Channel::shared_pointer channel = provider->createChannel(pvs[n], channelRequesterImpl);
            */
            
            Channel::shared_pointer channel = channels[n];
            shared_ptr<ChannelRequesterImpl> channelRequesterImpl = dynamic_pointer_cast<ChannelRequesterImpl>(channel->getChannelRequester());

            if (channelRequesterImpl->waitUntilConnected(timeOut))
            {
                shared_ptr<GetFieldRequesterImpl> getFieldRequesterImpl;

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
                                dynamic_pointer_cast<const Structure>(getFieldRequesterImpl->getField());
                        if (structure.get() == 0 || structure->getField("value").get() == 0)
                        {
                            // fallback to structure
                            mode = StructureMode;
                            pvRequest = getCreateRequest()->createRequest("field()", requester);
                        }
                    }

                    shared_ptr<ChannelGetRequesterImpl> getRequesterImpl(
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
                            printValue(channel->getChannelName(), getRequesterImpl->getPVStructure());
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

        if (collectValues)
            printValues(collectedNames, collectedValues);

        ClientFactory::stop();
    }
    // service RPC mode
    else
    {
        String authority;

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
                getCreateRequest()->createRequest(request, requester);
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
                        "uri:ev4:nt/2012/pwd:NTURI",
                        uriFieldNames,
                        uriFields
                        )
                    );



        PVStructure::shared_pointer request(
                    getPVDataCreate()->createPVStructure(uriStructure)
                    );

        request->getStringField("scheme")->put("pva");
        if (!authority.empty()) request->getStringField("authority")->put(authority);
        request->getStringField("path")->put(service);
        PVStructure::shared_pointer query = request->getStructureField("query");
        for (vector< pair<string, string> >::iterator iter = parameters.begin();
             iter != parameters.end();
             iter++)
        {
            query->getStringField(iter->first)->put(iter->second);
        }


        PVStructure::shared_pointer arg = onlyQuery ? query : request;
        if (debug)
        {
            std::cout << "Request structure: " << std::endl << *(arg.get()) << std::endl;
        }


        ClientFactory::start();
        ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("pvAccess");
        
        shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl(quiet));
        Channel::shared_pointer channel =
                authority.empty() ?
                    provider->createChannel(service, channelRequesterImpl) :
                    provider->createChannel(service, channelRequesterImpl,
                                            ChannelProvider::PRIORITY_DEFAULT, authority);
        
        if (channelRequesterImpl->waitUntilConnected(timeOut))
        {
            shared_ptr<ChannelRPCRequesterImpl> rpcRequesterImpl(new ChannelRPCRequesterImpl(channel->getChannelName()));
            ChannelRPC::shared_pointer channelRPC = channel->createChannelRPC(rpcRequesterImpl, pvRequest);

            if (rpcRequesterImpl->waitUntilConnected(timeOut))
            {
                channelRPC->request(arg, true);
                allOK &= rpcRequesterImpl->waitUntilRPC(timeOut);
                if (allOK)
                {
                    if (dumpStructure)
                    {
                        if (rpcRequesterImpl->getLastResponse().get() == 0)
                            std::cout << "(null)" << std::endl;
                        else
                            std::cout << *(rpcRequesterImpl->getLastResponse().get()) << std::endl;
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

        epics::pvAccess::ca::CAClientFactory::stop();
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

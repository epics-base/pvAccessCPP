#include "epicsv4Grayscale.h"

epics::pvData::StructureConstPtr createNTNDArrayStructure()
{
    static epics::pvData::StructureConstPtr ntndArrayStructure;

    if (!ntndArrayStructure.get())
    {
        StandardFieldPtr standardField = getStandardField();
        FieldBuilderPtr fb = getFieldCreate()->createFieldBuilder();

        for (int i = pvBoolean; i < pvString; ++i)
        {
            ScalarType st = static_cast<ScalarType>(i);
            fb->addArray(std::string(ScalarTypeFunc::name(st)) + "Value", st);
        }
        UnionConstPtr valueType = fb->createUnion();

        StructureConstPtr codecStruc = fb->setId("codec_t")->
                                       add("name", pvString)->
                                       add("parameters", getFieldCreate()->createVariantUnion())->
                                       createStructure();

        StructureConstPtr dimensionStruc = fb->setId("dimension_t")->
                                           add("size", pvInt)->
                                           add("offset",  pvInt)->
                                           add("fullSize",  pvInt)->
                                           add("binning",  pvInt)->
                                           add("reverse",  pvBoolean)->
                                           createStructure();

        StructureConstPtr attributeStruc = fb->setId("epics:nt/NTAttribute:1.0")->
                                           add("name", pvString)->
                                           add("value", getFieldCreate()->createVariantUnion())->
                                           add("descriptor", pvString)->
                                           add("sourceType", pvInt)->
                                           add("source", pvString)->
                                           createStructure();


        ntndArrayStructure = fb->setId("epics:nt/NTNDArray:1.0")->
                             add("value", valueType)->
                             add("codec", codecStruc)->
                             add("compressedSize", pvLong)->
                             add("uncompressedSize", pvLong)->
                             addArray("dimension", dimensionStruc)->
                             add("uniqueId", pvInt)->
                             add("dataTimeStamp", standardField->timeStamp())->
                             addArray("attribute", attributeStruc)->
                             //add("descriptor", pvString)->
                             //add("timeStamp", standardField->timeStamp())->
                             //add("alarm", standardField->alarm())->
                             //add("display", standardField->display())->
                             createStructure();
    }

    return ntndArrayStructure;
}

void setNTNDArrayValue(
    PVStructure::shared_pointer const & imagePV,
    const size_t raw_dim_size,
    const int32_t* raw_dim,
    const size_t raw_size,
    const int8_t* raw
)
{
    PVUnionPtr unionValue = imagePV->getSubField<PVUnion>("value");
    // assumes byteArray
    PVByteArrayPtr pvField = unionValue->select<PVByteArray>("byteValue");

    const int8_t *data = raw;
    size_t dataSize = raw_size;

    PVByteArray::svector temp(pvField->reuse());
    temp.resize(dataSize);
    if (data)
        std::copy(data, data + dataSize, temp.begin());
    pvField->replace(freeze(temp));

    PVStructureArrayPtr dimField = imagePV->getSubField<PVStructureArray>("dimension");

    PVStructureArray::svector dimVector(dimField->reuse());
    dimVector.resize(raw_dim_size);
    for (size_t i = 0; i < raw_dim_size; i++)
    {
        PVStructurePtr d = dimVector[i];
        if (!d)
            d = dimVector[i] = getPVDataCreate()->createPVStructure(dimField->getStructureArray()->getStructure());
        d->getSubField<PVInt>("size")->put(raw_dim[i]);
        d->getSubField<PVInt>("offset")->put(0);
        d->getSubField<PVInt>("fullSize")->put(raw_dim[i]);
        d->getSubField<PVInt>("binning")->put(1);
        d->getSubField<PVBoolean>("reverse")->put(false);
    }
    dimField->replace(freeze(dimVector));

    imagePV->getSubField<PVLong>("uncompressedSize")->put(static_cast<int64>(raw_size));

    imagePV->getSubField<PVLong>("compressedSize")->put(static_cast<int64>(raw_size));

    PVTimeStamp timeStamp;
    timeStamp.attach(imagePV->getSubField<PVStructure>("dataTimeStamp"));
    TimeStamp current;
    current.getCurrent();
    timeStamp.set(current);
}



void setNTNDArrayData(
    PVStructure::shared_pointer const & imagePV,
    const string & codec,
    int32 colorMode
)
{
    imagePV->getSubField<PVString>("codec.name")->put(codec);

    imagePV->getSubField<PVInt>("uniqueId")->put(0);

    PVStructureArray::shared_pointer pvAttributes = imagePV->getSubField<PVStructureArray>("attribute");

    PVStructureArray::svector attributes(pvAttributes->reuse());

    bool addNew = false;

    PVStructure::shared_pointer attribute;

    // find ColorMode
    for (PVStructureArray::const_svector::const_iterator iter = attributes.begin();
            iter != attributes.end();
            iter++)
    {
        PVStructure::shared_pointer fattribute = *iter;
        PVString::shared_pointer pvName = fattribute->getSubField<PVString>("name");
        if (pvName && pvName->get() == "ColorMode")
        {
            attribute = fattribute;
            break;
        }
    }

    if (!attribute)
    {
        attribute = getPVDataCreate()->createPVStructure(pvAttributes->getStructureArray()->getStructure());
        addNew = true;
    }

    attribute->getSubField<PVString>("name")->put("ColorMode");
    PVInt::shared_pointer pvColorMode = getPVDataCreate()->createPVScalar<PVInt>();
    pvColorMode->put(colorMode);

    if (addNew)
        attributes.push_back(attribute);

    attribute->getSubField<PVUnion>("value")->set(pvColorMode);
    attribute->getSubField<PVString>("descriptor")->put("Color mode");
    attribute->getSubField<PVInt>("sourceType")->put(0);
    attribute->getSubField<PVString>("source")->put("");

    pvAttributes->replace(freeze(attributes));
}

void initImage(
    PVStructure::shared_pointer const & imagePV,
    const string & codec,
    int32 colorMode,
    const size_t raw_dim_size,
    const int32_t* raw_dim,
    const size_t raw_size,
    const int8_t* raw
)
{
    setNTNDArrayValue(imagePV, raw_dim_size, raw_dim, raw_size, raw);
    setNTNDArrayData(imagePV, codec, colorMode);
}

void initImageEPICSv4GrayscaleLogo(PVStructure::shared_pointer const & imagePV)
{
    setNTNDArrayValue(imagePV, 2, epicsv4_raw_dim, epicsv4_raw_size, epicsv4_raw);
    setNTNDArrayData(imagePV, "", 0 /* NDColorModeMono=0 */);
}

void rotateImage(PVStructure::shared_pointer const & imagePV, const int8_t* originalImage, float deg)
{
    PVUnionPtr unionValue = imagePV->getSubField<PVUnion>("value");
    PVStructureArrayPtr dim = imagePV->getSubField<PVStructureArray>("dimension");

    PVStructureArray::const_svector data = dim->view();
    // 2d NTNDArray - { x, y }
    int32 cols = data[0]->getSubField<PVInt>("size")->get();
    int32 rows = data[1]->getSubField<PVInt>("size")->get();

    // assumes byteArray
    PVByteArrayPtr array = unionValue->get<PVByteArray>();

    double fi = 3.141592653589793238462 * deg / 180.0;
    double cosFi = 16.0 * cos(fi);
    double sinFi = 16.0 * sin(fi);

    int32 cx = cols/2;
    int32 cy = rows/2;

    int32 colsm2 = cols-2;
    int32 rowsm2 = rows-2;

    PVByteArray::svector imgData(array->reuse());
    int8_t* img = imgData.data();

    for (int32 y = 0; y < rows; y++)
    {
        int8_t* imgline = img + y*cols;
        int32 dcy = y - cy;
        for (int32 x = 0; x < cols; x++)
        {
            int32 dcx = x - cx;

            int32 tnx = static_cast<int32>(cosFi*dcx + sinFi*dcy);
            int32 tny = static_cast<int32>(-sinFi*dcx + cosFi*dcy);

            int32 nx = (tnx >> 4) + cx;
            int32 ny = (tny >> 4) + cy;

            if (nx < 0 || ny < 0 || nx > colsm2 || ny > rowsm2)
            {
                imgline[x] = 0;
            }
            else
            {
                const int8_t* srcline = originalImage + ny*cols;

                int32 xf = tnx & 0x0F;
                int32 yf = tny & 0x0F;

                int32 v00 = (16 - xf) * (16 - yf) * (srcline[nx] + 128);
                int32 v10 = xf * (16 - yf) * (srcline[nx + 1] + 128);
                int32 v01 = (16 - xf) * yf * (srcline[cols + nx] + 128);
                int32 v11 = xf * yf * (srcline[cols + nx + 1] + 128);
                uint8_t val = static_cast<uint8_t>((v00 + v01 + v10 + v11 + 128) / 256);
                imgline[x] = static_cast<int32>(val) - 128;

            }
        }
    }
    array->replace(freeze(imgData));

    PVIntPtr uniqueIdField = imagePV->getSubField<PVInt>("uniqueId");
    uniqueIdField->put(uniqueIdField->get()+1);

}

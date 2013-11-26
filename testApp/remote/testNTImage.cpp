#include "epicsv4Grayscale.h"

epics::pvData::StructureConstPtr makeVariantArrayStruc()
{
    FieldConstPtrArray vaFields;
    StringArray vaNames;

    vaFields.push_back(getFieldCreate()->createScalar(epics::pvData::pvInt));
    vaNames.push_back("dataType");

    vaFields.push_back(getFieldCreate()->createScalarArray(epics::pvData::pvInt));
    vaNames.push_back("value");

    epics::pvData::StructureConstPtr varrayStruc = getFieldCreate()->createStructure("uri:ev4:nt/2012/pwd:NTVariantArray", vaNames, vaFields);

    return varrayStruc;
}

epics::pvData::StructureConstPtr makeImageStruc()
{
    static epics::pvData::StructureConstPtr imageStruc;

    if (imageStruc == NULL)
    {
        FieldConstPtrArray fields;
        StringArray names;

        // Array part
        fields.push_back(getFieldCreate()->createScalarArray(epics::pvData::pvByte));
        names.push_back("value");

        fields.push_back(getFieldCreate()->createScalarArray(epics::pvData::pvInt));
        names.push_back("dim");

        // Image part
        fields.push_back(getFieldCreate()->createScalar(epics::pvData::pvInt));
        names.push_back("colorMode");

        fields.push_back(getFieldCreate()->createScalar(epics::pvData::pvInt));
        names.push_back("bayerPattern");

        fields.push_back(getFieldCreate()->createScalar(epics::pvData::pvString));
        names.push_back("fourcc");

        fields.push_back(getFieldCreate()->createScalarArray(epics::pvData::pvInt));
        names.push_back("offset");

        fields.push_back(getFieldCreate()->createScalarArray(epics::pvData::pvInt));
        names.push_back("binning");

        fields.push_back(getFieldCreate()->createScalarArray(epics::pvData::pvInt));
        names.push_back("reverse");

        fields.push_back(getFieldCreate()->createScalarArray(epics::pvData::pvInt));
        names.push_back("fullDim");


        // Metadata part
        fields.push_back(getFieldCreate()->createScalar(epics::pvData::pvInt));
        names.push_back("uniqueId");

        fields.push_back(getFieldCreate()->createScalarArray(epics::pvData::pvInt));
        names.push_back("attributeSourceTypes");

        fields.push_back(getFieldCreate()->createScalarArray(epics::pvData::pvString));
        names.push_back("attributeSources");

        fields.push_back(getFieldCreate()->createStructureArray(makeVariantArrayStruc()));
        names.push_back("attributes");

        imageStruc = getFieldCreate()->createStructure("uri:ev4:nt/2012/pwd:NTImage", names, fields);
    }


    return imageStruc;
}

void setImageArrayValues(
        PVStructure::shared_pointer const & imagePV,
        const size_t raw_dim_size,
        const int32_t* raw_dim,
        const size_t raw_size,
        const int8_t* raw
        )
{
    String id = imagePV->getStructure()->getID();
    PVByteArrayPtr pvField = static_pointer_cast<PVByteArray>(imagePV->getSubField("value"));

    const int8_t *data = raw;
    size_t dataSize = raw_size;

    PVByteArray::svector temp(pvField->reuse());
    temp.resize(dataSize);
    if (data)
        std::copy(data, data + dataSize, temp.begin());
    pvField->replace(freeze(temp));

	PVIntArrayPtr dimField = static_pointer_cast<PVIntArray>(
	   imagePV->getScalarArrayField(String("dim"), pvInt));

    const int32_t *dim = raw_dim;
    PVIntArray::svector temp2(dimField->reuse());
	temp2.resize(raw_dim_size);
	std::copy(dim, dim + raw_dim_size, temp2.begin());
	dimField->replace(freeze(temp2));
}


void setImageImageValues(
        PVStructure::shared_pointer const & imagePV,
        const int32_t colorMode,
        const size_t raw_dim_size,
        const int32_t* raw_dim
        )
{
    PVIntPtr colorModeField = imagePV->getIntField(String("colorMode"));
    colorModeField->put(colorMode);

    PVIntArrayPtr offsetField = static_pointer_cast<PVIntArray>(
        imagePV->getScalarArrayField(String("offset"), pvInt));
    PVIntArray::svector temp(offsetField->reuse());
    temp.resize(2);
    int32_t offsets[] = { 0, 0 };
    std::copy(offsets, offsets + 2, temp.begin());
    offsetField->replace(freeze(temp));

    PVIntArrayPtr binningField = static_pointer_cast<PVIntArray>(
        imagePV->getScalarArrayField(String("binning"), pvInt));
    temp = binningField->reuse();
    temp.resize(2);
    int32_t binnings[] = { 1, 1 };
    std::copy(binnings, binnings + 2, temp.begin());
    binningField->replace(freeze(temp));

    PVIntArrayPtr reverseField = static_pointer_cast<PVIntArray>(
        imagePV->getScalarArrayField(String("reverse"), pvInt));
        reverseField->setCapacity(2);
    temp = reverseField->reuse();
    temp.resize(2);
    int32_t reverses[] = { 0, 0 };
    std::copy(reverses, reverses + 2, temp.begin());
    reverseField->replace(freeze(temp));

	PVIntArrayPtr fullDimField = static_pointer_cast<PVIntArray>(
        imagePV->getScalarArrayField(String("fullDim"), pvInt));
    temp = fullDimField->reuse();
    temp.resize(raw_dim_size);
    const int32_t *fullDim = raw_dim;
    std::copy(fullDim, fullDim + raw_dim_size, temp.begin());
    fullDimField->replace(freeze(temp));
}


void setImageUniqueId(PVStructure::shared_pointer const & imagePV)
{
	PVIntPtr uniqueIdField = imagePV->getIntField(String("uniqueId"));
	uniqueIdField->put(0);
}

void setImageMetadataValues(PVStructure::shared_pointer const & imagePV)
{
    setImageUniqueId(imagePV);
}


void initImage(
        PVStructure::shared_pointer const & imagePV,
        const int32_t colorMode,
        const size_t raw_dim_size,
        const int32_t* raw_dim,
        const size_t raw_size,
        const int8_t* raw
        )
{
    setImageArrayValues(imagePV, raw_dim_size, raw_dim, raw_size, raw);
    setImageImageValues(imagePV, colorMode, raw_dim_size, raw_dim);
    setImageMetadataValues(imagePV);
}

void initImageEPICSv4GrayscaleLogo(PVStructure::shared_pointer const & imagePV)
{
    setImageArrayValues(imagePV, 2, epicsv4_raw_dim, epicsv4_raw_size, epicsv4_raw);
    setImageImageValues(imagePV, 0 /* monochrome */, 2 /* 2d image */, epicsv4_raw_dim);
    setImageMetadataValues(imagePV);
}

void rotateImage(PVStructure::shared_pointer const & imagePV, const int8_t* originalImage, float deg)
{
    PVScalarArrayPtr value = static_pointer_cast<PVScalarArray>(imagePV->getSubField("value"));
    PVIntArrayPtr dim = static_pointer_cast<PVIntArray>(imagePV->getScalarArrayField("dim", pvInt));

    PVIntArray::const_svector data = dim->view();
    // dim[] = { rows, columns }
    int32 cols = data[0];
    int32 rows = data[1];

    PVByteArrayPtr array = static_pointer_cast<PVByteArray>(value);

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

    PVIntPtr uniqueIdField = imagePV->getIntField(String("uniqueId"));
    uniqueIdField->put(uniqueIdField->get()+1);

}

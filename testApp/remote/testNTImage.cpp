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

    size_t dataSize = raw_size;
	pvField->setCapacity(dataSize);
    const int8_t *data = raw;
    if (data)
        pvField->put(0, dataSize, data, 0);

	PVIntArrayPtr dimField = static_pointer_cast<PVIntArray>(
	   imagePV->getScalarArrayField(String("dim"), pvInt));
    dimField->setCapacity(raw_dim_size);
    const int32_t *dim = raw_dim;
    dimField->put(0, raw_dim_size, dim, 0);
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
    offsetField->setCapacity(2);
    int32_t offsets[] = { 0, 0 };
	offsetField->put(0, 2, offsets, 0);

    PVIntArrayPtr binningField = static_pointer_cast<PVIntArray>(
        imagePV->getScalarArrayField(String("binning"), pvInt));
    binningField->setCapacity(2);
    int32_t binnings[] = { 1, 1 };
    binningField->put(0, 2, binnings, 0);

    PVIntArrayPtr reverseField = static_pointer_cast<PVIntArray>(
        imagePV->getScalarArrayField(String("reverse"), pvInt));
        reverseField->setCapacity(2);
    int32_t reverses[] = { 0, 0 };
	reverseField->put(0, 2, reverses, 0);

	PVIntArrayPtr fullDimField = static_pointer_cast<PVIntArray>(
        imagePV->getScalarArrayField(String("fullDim"), pvInt));
    fullDimField->setCapacity(raw_dim_size);
    const int32_t *fullDim = raw_dim;
    fullDimField->put(0, raw_dim_size, fullDim, 0);

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
    // dim[] = { rows, columns }
    int32 rows, cols;
    size_t dims = dim->getLength();

    IntArrayData data;
    dim->get(0, dims, data);
    cols = data.data[0];
    rows = data.data[1];

    PVByteArrayPtr array = static_pointer_cast<PVByteArray>(value);

    double fi = 3.141592653589793238462 * deg / 180.0;
    double cosFi = 16.0 * cos(fi);
    double sinFi = 16.0 * sin(fi);

    int32 cx = cols/2;
    int32 cy = rows/2;

    int32 colsm2 = cols-2;
    int32 rowsm2 = rows-2;

    int8_t* img = array->get();

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

    PVIntPtr uniqueIdField = imagePV->getIntField(String("uniqueId"));
    uniqueIdField->put(uniqueIdField->get()+1);

}

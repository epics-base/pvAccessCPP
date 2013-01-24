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


void setImageArrayValues(PVStructure::shared_pointer const & imagePV)
{
    String id = imagePV->getStructure()->getID();
    PVByteArrayPtr pvField = static_pointer_cast<PVByteArray>(imagePV->getSubField("value"));

	size_t dataSize = epicsv4_raw_size;
	pvField->setCapacity(dataSize);
	const int8_t *data = epicsv4_raw;
	pvField->put(0, dataSize, data, 0);

	PVIntArrayPtr dimField = static_pointer_cast<PVIntArray>(
	   imagePV->getScalarArrayField(String("dim"), pvInt));
	dimField->setCapacity(2);
	const int32_t *dim = epicsv4_raw_dim;
	dimField->put(0, 2, dim, 0);
}


void setImageImageValues(PVStructure::shared_pointer const & imagePV)
{
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
    fullDimField->setCapacity(2);
    const int32_t *fullDim = epicsv4_raw_dim;
	fullDimField->put(0, 2, fullDim, 0);

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


void initImage(PVStructure::shared_pointer const & imagePV)
{
    setImageArrayValues(imagePV);
    setImageImageValues(imagePV);
    setImageMetadataValues(imagePV);
}


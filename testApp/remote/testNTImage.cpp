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

void rotateImage(PVStructure::shared_pointer const & imagePV, float deg)
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

    ByteArrayData dimg;
    size_t imgSize = array->getLength();
    array->get(0, imgSize, dimg);

    double fi = 3.141592653589793238462 * deg / 180.0;
    double cosFi = 16.0 * cos(fi);
    double sinFi = 16.0 * sin(fi);

    int32 cx = cols/2;
    int32 cy = rows/2;

    int32 colsm2 = cols-2;
    int32 rowsm2 = rows-2;

    int8_t* img = &dimg.data[0];

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
                const int8_t* srcline = epicsv4_raw + ny*cols;

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
    array->put(0, imgSize, img, 0);

    PVIntPtr uniqueIdField = imagePV->getIntField(String("uniqueId"));
    uniqueIdField->put(uniqueIdField->get()+1);

}


void
rotateAMColorFastLow(uint32  *datad,
                     int32    w,
                     int32    h,
                     int32    wpld,
                     uint32  *datas,
                     int32    wpls,
                     float  angle,
                     uint32   colorval);

void rotateRGBImage(PVStructure::shared_pointer const & imagePV, float deg)
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

    PVIntArrayPtr array = static_pointer_cast<PVIntArray>(value);

    IntArrayData dimg;
    size_t imgSize = array->getLength();
    array->get(0, imgSize, dimg);

    rotateAMColorFastLow((uint32*)&dimg.data[0],
                         cols,
                         rows,
                         cols,
                         (uint32*)&dimg.data[0],
                         cols,
                         3.141592653589793238462 * deg / 180.0,
                         0x00000000);

    array->put(0, imgSize, &dimg.data[0], 0);

    PVIntPtr uniqueIdField = imagePV->getIntField(String("uniqueId"));
    uniqueIdField->put(uniqueIdField->get()+1);

}

/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/


#include <string.h>
#include <math.h>

/*------------------------------------------------------------------*
 *               Fast RGB color rotation about center               *
 *------------------------------------------------------------------*/
/*!
 *  rotateAMColorFastLow()
 *
 *     This is a special simplification of area mapping with division
 *     of each pixel into 16 sub-pixels.  The exact coefficients that
 *     should be used are the same as for the 4x linear interpolation
 *     scaling case, and are given there.  I tried to approximate these
 *     as weighted coefficients with a maximum sum of 4, which
 *     allows us to do the arithmetic in parallel for the R, G and B
 *     components in a 32 bit pixel.  However, there are three reasons
 *     for not doing that:
 *        (1) the loss of accuracy in the parallel implementation
 *            is visually significant
 *        (2) the parallel implementation (described below) is slower
 *        (3) the parallel implementation requires allocation of
 *            a temporary color image
 *
 *     There are 16 cases for the choice of the subpixel, and
 *     for each, the mapping to the relevant source
 *     pixels is as follows:
 *
 *      subpixel      src pixel weights
 *      --------      -----------------
 *         0          sp1
 *         1          (3 * sp1 + sp2) / 4
 *         2          (sp1 + sp2) / 2
 *         3          (sp1 + 3 * sp2) / 4
 *         4          (3 * sp1 + sp3) / 4
 *         5          (9 * sp1 + 3 * sp2 + 3 * sp3 + sp4) / 16
 *         6          (3 * sp1 + 3 * sp2 + sp3 + sp4) / 8
 *         7          (3 * sp1 + 9 * sp2 + sp3 + 3 * sp4) / 16
 *         8          (sp1 + sp3) / 2
 *         9          (3 * sp1 + sp2 + 3 * sp3 + sp4) / 8
 *         10         (sp1 + sp2 + sp3 + sp4) / 4
 *         11         (sp1 + 3 * sp2 + sp3 + 3 * sp4) / 8
 *         12         (sp1 + 3 * sp3) / 4
 *         13         (3 * sp1 + sp2 + 9 * sp3 + 3 * sp4) / 16
 *         14         (sp1 + sp2 + 3 * sp3 + 3 * sp4) / 8
 *         15         (sp1 + 3 * sp2 + 3 * sp3 + 9 * sp4) / 16
 *
 *     Another way to visualize this is to consider the area mapping
 *     (or linear interpolation) coefficients  for the pixel sp1.
 *     Expressed in fourths, they can be written as asymmetric matrix:
 *
 *           4      3      2      1
 *           3      2.25   1.5    0.75
 *           2      1.5    1      0.5
 *           1      0.75   0.5    0.25
 *
 *     The coefficients for the three neighboring pixels can be
 *     similarly written.
 *
 *     This is implemented here, where, for each color component,
 *     we inline its extraction from each participating word,
 *     construct the linear combination, and combine the results
 *     into the destination 32 bit RGB pixel, using the appropriate shifts.
 *
 *     It is interesting to note that an alternative method, where
 *     we do the arithmetic on the 32 bit pixels directly (after
 *     shifting the components so they won't overflow into each other)
 *     is significantly inferior.  Because we have only 8 bits for
 *     internal overflows, which can be distributed as 2, 3, 3, it
 *     is impossible to add these with the correct linear
 *     interpolation coefficients, which require a sum of up to 16.
 *     Rounding off to a sum of 4 causes appreciable visual artifacts
 *     in the rotated image.  The code for the inferior method
 *     can be found in prog/rotatefastalt.c, for reference.
 *
 *     *** Warning: explicit assumption about RGB component ordering ***
 */
void
rotateAMColorFastLow(uint32  *datad,
                     int32    w,
                     int32    h,
                     int32    wpld,
                     uint32  *datas,
                     int32    wpls,
                     float  angle,
                     uint32   colorval)
{
int32    i, j, xcen, ycen, wm2, hm2;
int32    xdif, ydif, xpm, ypm, xp, yp, xf, yf;
uint32   word1, word2, word3, word4, red, blue, green;
uint32  *pword, *lines, *lined;
float  sina, cosa;

    xcen = w / 2;
    wm2 = w - 2;
    ycen = h / 2;
    hm2 = h - 2;
    sina = 4. * sin(angle);
    cosa = 4. * cos(angle);

    for (i = 0; i < h; i++) {
        ydif = ycen - i;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            xdif = xcen - j;
            xpm = (int32)(-xdif * cosa - ydif * sina);
            ypm = (int32)(-ydif * cosa + xdif * sina);
            xp = xcen + (xpm >> 2);
            yp = ycen + (ypm >> 2);
            xf = xpm & 0x03;
            yf = ypm & 0x03;

                /* if off the edge, write input grayval */
            if (xp < 0 || yp < 0 || xp > wm2 || yp > hm2) {
                *(lined + j) = colorval;
                continue;
            }

            lines = datas + yp * wpls;
            pword = lines + xp;

            switch (xf + 4 * yf)
            {
            case 0:
                *(lined + j) = *pword;
                break;
            case 1:
                word1 = *pword;
                word2 = *(pword + 1);
                red = 3 * (word1 >> 24) + (word2 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) +
                            ((word2 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) +
                            ((word2 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 2:
                word1 = *pword;
                word2 = *(pword + 1);
                red = (word1 >> 24) + (word2 >> 24);
                green = ((word1 >> 16) & 0xff) + ((word2 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff);
                *(lined + j) = ((red << 23) & 0xff000000) |
                               ((green << 15) & 0x00ff0000) |
                               ((blue << 7) & 0x0000ff00);
                break;
            case 3:
                word1 = *pword;
                word2 = *(pword + 1);
                red = (word1 >> 24) + 3 * (word2 >> 24);
                green = ((word1 >> 16) & 0xff) +
                          3 * ((word2 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) +
                          3 * ((word2 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 4:
                word1 = *pword;
                word3 = *(pword + wpls);
                red = 3 * (word1 >> 24) + (word3 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) +
                            ((word3 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) +
                            ((word3 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 5:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 9 * (word1 >> 24) + 3 * (word2 >> 24) +
                      3 * (word3 >> 24) + (word4 >> 24);
                green = 9 * ((word1 >> 16) & 0xff) +
                        3 * ((word2 >> 16) & 0xff) +
                        3 * ((word3 >> 16) & 0xff) +
                        ((word4 >> 16) & 0xff);
                blue = 9 * ((word1 >> 8) & 0xff) +
                       3 * ((word2 >> 8) & 0xff) +
                       3 * ((word3 >> 8) & 0xff) +
                       ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 20) & 0xff000000) |
                               ((green << 12) & 0x00ff0000) |
                               ((blue << 4) & 0x0000ff00);
                break;
            case 6:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 3 * (word1 >> 24) +  3 * (word2 >> 24) +
                      (word3 >> 24) + (word4 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) +
                        3 * ((word2 >> 16) & 0xff) +
                        ((word3 >> 16) & 0xff) +
                        ((word4 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) +
                       3 * ((word2 >> 8) & 0xff) +
                       ((word3 >> 8) & 0xff) +
                       ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 21) & 0xff000000) |
                               ((green << 13) & 0x00ff0000) |
                               ((blue << 5) & 0x0000ff00);
                break;
            case 7:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 3 * (word1 >> 24) + 9 * (word2 >> 24) +
                      (word3 >> 24) + 3 * (word4 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) +
                        9 * ((word2 >> 16) & 0xff) +
                        ((word3 >> 16) & 0xff) +
                        3 * ((word4 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) +
                       9 * ((word2 >> 8) & 0xff) +
                         ((word3 >> 8) & 0xff) +
                         3 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 20) & 0xff000000) |
                               ((green << 12) & 0x00ff0000) |
                               ((blue << 4) & 0x0000ff00);
                break;
            case 8:
                word1 = *pword;
                word3 = *(pword + wpls);
                red = (word1 >> 24) + (word3 >> 24);
                green = ((word1 >> 16) & 0xff) + ((word3 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + ((word3 >> 8) & 0xff);
                *(lined + j) = ((red << 23) & 0xff000000) |
                               ((green << 15) & 0x00ff0000) |
                               ((blue << 7) & 0x0000ff00);
                break;
            case 9:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 3 * (word1 >> 24) + (word2 >> 24) +
                      3 * (word3 >> 24) + (word4 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) + ((word2 >> 16) & 0xff) +
                        3 * ((word3 >> 16) & 0xff) + ((word4 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff) +
                       3 * ((word3 >> 8) & 0xff) + ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 21) & 0xff000000) |
                               ((green << 13) & 0x00ff0000) |
                               ((blue << 5) & 0x0000ff00);
                break;
            case 10:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = (word1 >> 24) + (word2 >> 24) +
                      (word3 >> 24) + (word4 >> 24);
                green = ((word1 >> 16) & 0xff) + ((word2 >> 16) & 0xff) +
                        ((word3 >> 16) & 0xff) + ((word4 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff) +
                       ((word3 >> 8) & 0xff) + ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 11:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = (word1 >> 24) + 3 * (word2 >> 24) +
                      (word3 >> 24) + 3 * (word4 >> 24);
                green = ((word1 >> 16) & 0xff) + 3 * ((word2 >> 16) & 0xff) +
                        ((word3 >> 16) & 0xff) + 3 * ((word4 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + 3 * ((word2 >> 8) & 0xff) +
                       ((word3 >> 8) & 0xff) + 3 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 21) & 0xff000000) |
                               ((green << 13) & 0x00ff0000) |
                               ((blue << 5) & 0x0000ff00);
                break;
            case 12:
                word1 = *pword;
                word3 = *(pword + wpls);
                red = (word1 >> 24) + 3 * (word3 >> 24);
                green = ((word1 >> 16) & 0xff) +
                          3 * ((word3 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) +
                          3 * ((word3 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 13:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 3 * (word1 >> 24) + (word2 >> 24) +
                      9 * (word3 >> 24) + 3 * (word4 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) + ((word2 >> 16) & 0xff) +
                        9 * ((word3 >> 16) & 0xff) + 3 * ((word4 >> 16) & 0xff);
                blue = 3 *((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff) +
                       9 * ((word3 >> 8) & 0xff) + 3 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 20) & 0xff000000) |
                               ((green << 12) & 0x00ff0000) |
                               ((blue << 4) & 0x0000ff00);
                break;
            case 14:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = (word1 >> 24) + (word2 >> 24) +
                      3 * (word3 >> 24) + 3 * (word4 >> 24);
                green = ((word1 >> 16) & 0xff) +((word2 >> 16) & 0xff) +
                        3 * ((word3 >> 16) & 0xff) + 3 * ((word4 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff) +
                       3 * ((word3 >> 8) & 0xff) + 3 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 21) & 0xff000000) |
                               ((green << 13) & 0x00ff0000) |
                               ((blue << 5) & 0x0000ff00);
                break;
            case 15:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = (word1 >> 24) + 3 * (word2 >> 24) +
                      3 * (word3 >> 24) + 9 * (word4 >> 24);
                green = ((word1 >> 16) & 0xff) + 3 * ((word2 >> 16) & 0xff) +
                        3 * ((word3 >> 16) & 0xff) + 9 * ((word4 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + 3 * ((word2 >> 8) & 0xff) +
                       3 * ((word3 >> 8) & 0xff) + 9 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 20) & 0xff000000) |
                               ((green << 12) & 0x00ff0000) |
                               ((blue << 4) & 0x0000ff00);
                break;
            default:
                fprintf(stderr, "shouldn't get here\n");
                break;
            }
        }
    }

    return;
}

/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

extern void pvCaAllTests(void);

int main(int argc, char **argv)
{
    pvCaAllTests();  /* calls epicsExit(0) */
    return 0;
}

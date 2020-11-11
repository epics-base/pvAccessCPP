/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

/* no data files needed */
char* epicsRtemsFSImage = 0;

extern void pvAccessAllTests(void);

int main(int argc, char **argv)
{
    pvAccessAllTests();  /* calls epicsExit(0) */
    return 0;
}

/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef _PVACCESSMB_H_
#define _PVACCESSMB_H_

/* this header is deprecated */

#define MB_DECLARE(NAME, SIZE)
#define MB_DECLARE_EXTERN(NAME)

#define MB_POINT_ID(NAME, STAGE, STAGE_DESC, ID)

#define MB_INC_AUTO_ID(NAME)
#define MB_POINT(NAME, STAGE, STAGE_DESC)

#define MB_POINT_CONDITIONAL(NAME, STAGE, STAGE_DESC, COND)

#define MB_NORMALIZE(NAME)

#define MB_STATS(NAME, STREAM)
#define MB_STATS_OPT(NAME, STAGE_ONLY, SKIP_FIRST_N_SAMPLES, STREAM)

#define MB_CSV_EXPORT(NAME, STREAM)
#define MB_CSV_EXPORT_OPT(NAME, STAGE_ONLY, SKIP_FIRST_N_SAMPLES, STREAM)
#define MB_CSV_IMPORT(NAME, STREAM)

#define MB_PRINT(NAME, STREAM)
#define MB_PRINT_OPT(NAME, STAGE_ONLY, SKIP_FIRST_N_SAMPLES, STREAM)

#define MB_INIT

#endif

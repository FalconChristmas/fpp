#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#ifdef __cplusplus
extern "C" {
#endif

const char* getFPPVersion(void);
const char* getFPPMajorVersion(void);
const char* getFPPMinorVersion(void);
const char* getFPPBranch(void);
const char* getFPPVersionTriplet(void);
void printVersionInfo(void);

#ifdef __cplusplus
}
#endif


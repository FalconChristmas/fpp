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

#define E131_HEADER_LENGTH 126
#define MAX_STEPS_OUT_OF_SYNC 2

#define E131_DEST_PORT 5568

#define E131_UNIVERSE_INDEX 113
#define E131_SEQUENCE_INDEX 111
#define E131_COUNT_INDEX 123
#define E131_START_CODE 125
#define E131_PRIORITY_INDEX 108

#define E131_RLP_COUNT_INDEX 16
#define E131_FRAMING_COUNT_INDEX 38
#define E131_DMP_COUNT_INDEX 115

#define E131_VECTOR_INDEX 21
#define E131_EXTENDED_PACKET_TYPE_INDEX 43
#define VECTOR_E131_EXTENDED_SYNCHRONIZATION 0x1
#define VECTOR_ROOT_E131_DATA 0x4
#define VECTOR_ROOT_E131_EXTENDED 0x8

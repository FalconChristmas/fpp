/*
 *   Pixel Overlay controller for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PIXELOVERLAYCONTROL_H
#define _PIXELOVERLAYCONTROL_H

#define FPPCHANNELMEMORYMAPMAJORVER 1
#define FPPCHANNELMEMORYMAPMINORVER 0
#define FPPCHANNELMEMORYMAPSIZE     131072

#define FPPCHANNELMEMORYMAPDATAFILE  "/var/tmp/FPPChannelData"
#define FPPCHANNELMEMORYMAPCTRLFILE  "/var/tmp/FPPChannelCtrl"
#define FPPCHANNELMEMORYMAPPIXELFILE "/var/tmp/FPPChannelPixelMap"

/*
 * Header block on channel data memory map control interface file.
 * We want the size of this to equal 256 bytes so we have room for
 * expansion and to allow the header and up to 255 map block definitions
 * in a 64K memory mapped file.
 */
typedef struct {
	unsigned char   majorVersion;     // major version of memory map layout
	unsigned char   minorVersion;     // minor version of memory map layout
	unsigned char   totalBlocks;      // number of blocks defined in config file
	unsigned char   testMode;         // 0/1, read by fppd, 1 == copy all channels
	unsigned char   filler[252];      // filler for future use
} FPPChannelMemoryMapControlHeader;

/*
 * NOTE: 'long long' are used here to make things easier cross-platform (32 vs 64-bit)
 *       Fields are also ordered by largest to smallest datatype for the same reason
 */
typedef struct {
	long long       startChannel;     // 1-based start channel
	long long       channelCount;     // number of channels to copy
	long long       stringCount;      // Number of strings
	long long       strandsPerString; // Number of strands per string (# of folds + 1)
	char            blockName[32];    // null-terminated string, set by fppd
	char            startCorner[3];   // TL, TR, BL, BR (Top/Bottom and Left/Right)
	unsigned char   isActive;         // 0/1 set by client, read by fppd
	char            orientation;      // 'H'orizontal or 'V'ertical
	unsigned char   isLocked;         // Suggested access lock between processes
	char            filler[186];      // filler for future use
} FPPChannelMemoryMapControlBlock;

#endif /* _MEMORYMAPCONTROL_H */

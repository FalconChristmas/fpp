/*
 *   Pixel Overlay handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
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

#ifndef _PIXELOVERLAY_H
#define _PIXELOVERLAY_H

#include <string>

int InitializeChannelDataMemoryMap(void);
int UsingMemoryMapInput(void);
void CloseChannelDataMemoryMap(void);
void OverlayMemoryMap(char *channelData);

bool GetPixelOverlayModelSize(const std::string &modelName, int &w, int &h);
void SetPixelOverlayData(const std::string &modelName, const uint8_t *data);

int SetPixelOverlayState(std::string modelName, std::string newState);
int SetPixelOverlayValue(int index, char value, int startChannel = -1, int endChannel = -1);
int SetPixelOverlayValue(std::string modelName, char value, int startChannel = -1, int endChannel = -1);

int FillPixelOverlayModel(int index, unsigned char r, unsigned char g, unsigned char b);
int FillPixelOverlayModel(std::string modelName, unsigned char r, unsigned char g, unsigned char b);

#endif /* _PIXELOVERLAY_H */

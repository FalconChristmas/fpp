/*
 *   Matrix class for the Falcon Player Daemon 
 *   Falcon Player project (FPP) 
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

#ifndef _MATRIX_H
#define _MATRIX_H

#include <vector>

typedef struct subMatrix {
	int enabled;
	int startChannel;
	int width;
	int height;
	int xOffset;
	int yOffset;
} SubMatrix;

class Matrix {
  public:
	Matrix(int startChannel, int width, int height);
	~Matrix();

	void AddSubMatrix(int enabled, int startChannel, int width, int height,
		int xOffset, int yOffset);

	void OverlaySubMatrix(unsigned char *channelData, int i);
	void OverlaySubMatrices(unsigned char *channelData);

  private:
	int  m_startChannel;
	int  m_width;
	int  m_height;
	int  m_enableFlagOffset;

	unsigned char *m_buffer;

	std::vector<SubMatrix>  subMatrix;
};

#endif

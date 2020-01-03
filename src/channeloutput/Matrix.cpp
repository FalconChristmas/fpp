/*
 *   Matrix class for the Falcon Player Daemon 
 *   Falcon Player project (FPP) 
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

#include <string.h>
#include <strings.h>

#include "log.h"
#include "common.h"
#include "Matrix.h"

/*
 *
 */
Matrix::Matrix(int startChannel, int width, int height)
  : m_startChannel(startChannel),
	m_width(width),
	m_height(height)
{
	LogDebug(VB_CHANNELOUT, "New Matrix: %dx%d\n", width, height);

	m_buffer = new unsigned char[width * height * 3];

	m_enableFlagOffset = startChannel + (width * height * 3);
}

/*
 *
 */
Matrix::~Matrix()
{
	delete [] m_buffer;
}

/*
 *
 */
void Matrix::AddSubMatrix(int enabled, int startChannel, int width, int height,
	int xOffset, int yOffset)
{
	LogDebug(VB_CHANNELOUT,
		"New SubMatrix: %dx%d+%d+%d @ channel %d, enabled: %d\n",
		 width, height, xOffset, yOffset, startChannel, enabled);
	SubMatrix newSub;

	newSub.enabled = enabled;
	newSub.startChannel = startChannel;
	newSub.width = width;
	newSub.height = height;
	newSub.xOffset = xOffset;
	newSub.yOffset = yOffset;

	subMatrix.push_back(newSub);
}

/*
 *
 */
void Matrix::OverlaySubMatrix(unsigned char *channelData, int i)
{
	if (subMatrix.size() < i)
		return;

	// Check to see if this submatrix is enabled
	if ((!subMatrix[i].enabled) &&
		(!channelData[m_enableFlagOffset + i]))
		return;

	int startChannel = subMatrix[i].startChannel;
	int xOffset = subMatrix[i].xOffset;
	int yOffset = subMatrix[i].yOffset;

	unsigned char *src;
	unsigned char *dest;

	for (int y = 0; y < subMatrix[i].height; y++)
	{
		src = channelData + subMatrix[i].startChannel + (y * subMatrix[i].width * 3);
		dest = m_buffer + ((y + subMatrix[i].yOffset) * m_width * 3) + (subMatrix[i].xOffset * 3);

		memcpy(dest, src, subMatrix[i].width * 3);
	}
}

/*
 *
 */
void Matrix::OverlaySubMatrices(unsigned char *channelData)
{
	if (!subMatrix.size())
		return;

	// Clear the matrix before applying submatrices
	// bzero(m_buffer, m_width * m_height * 3);

	// Instead, just populate our temp buffer with the contents of the matrix
	memcpy(m_buffer, channelData + m_startChannel, m_width * m_height * 3);

	for (int i = 0; i < subMatrix.size(); i++)
	{
		OverlaySubMatrix(channelData, i);
	}

	memcpy(channelData + m_startChannel, m_buffer, m_width * m_height * 3);
}


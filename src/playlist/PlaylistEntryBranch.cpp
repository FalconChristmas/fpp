/*
 *   Playlist Entry Branch Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2016 the Falcon Player Developers
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

#include "PlaylistEntryBranch.h"

PlaylistEntryBranch::PlaylistEntryBranch()
  : m_branchType(PE_BRANCH_TYPE_UNDEFINED),
	m_comparisonMode(PE_BRANCH_COMP_MODE_UNDEFINED),
	m_hour(0),
	m_minute(0),
	m_second(0),
	m_loopCount(0),
	m_branch(NULL)
{
}

PlaylistEntryBranch::~PlaylistEntryBranch()
{
}


/*
 *   ScheduleEntry class for the Falcon Player (FPP) 
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

#ifndef _SCHEDULEENTRY_H
#define _SCHEDULEENTRY_H

#include <string>

#include <time.h>

#include "Playlist.h"

#define DAYS_PER_WEEK       7

class ScheduleEntry {
  public:
	ScheduleEntry();
	~ScheduleEntry();

	int  LoadFromString(std::string entryStr);
	void CalculateTimes(void);

	typedef enum {
		SS_IDLE = 0,
		SS_PLAYING,
		SS_STOPPING
	} ScheduleState;

	int          m_enabled;
	std::string  m_playlistName;
	OldPlaylist    *m_playlist;
	int          m_priority;
	int          m_repeating;
	int          m_dayIndex;
	int          m_weeklySecondCount;
	int          m_weeklyStartSeconds[DAYS_PER_WEEK];
	int          m_weeklyEndSeconds[DAYS_PER_WEEK];
	int          m_startTime; // HHMMSS format as an integer
	int          m_endTime;   // HHMMSS format as an integer
	int          m_startHour;
	int          m_startMinute;
	int          m_startSecond;
	int          m_endHour;
	int          m_endMinute;
	int          m_endSecond;
	int          m_startDate; // YYYYMMDD format as an integer
	int          m_endDate;   // YYYYMMDD format as an integer

	ScheduleState m_state;

	// The last start/stop block
	time_t m_lastStartTime;
	time_t m_lastEndTime;

	// The current start/stop block. if not playing then == next
	time_t m_thisStartTime;
	time_t m_thisEndTime;

	// The next scheduled start/stop block
	time_t m_nextStartTime;
	time_t m_nextEndTime;

};

#endif /* _SCHEDULEENTRY_H */

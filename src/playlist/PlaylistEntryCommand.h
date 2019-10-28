/*
 *   Playlist Entry Command Class for Falcon Player (FPP)
 * *
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

#ifndef _PLAYLISTENTRYCOMMAND_H
#define _PLAYLISTENTRYCOMMAND_H

#include <string>
#include <vector>

#include "PlaylistEntryBase.h"
#include "commands/Commands.h"

class PlaylistEntryCommand : public PlaylistEntryBase {
  public:
	PlaylistEntryCommand(PlaylistEntryBase *parent = NULL);
	~PlaylistEntryCommand();

	int  Init(Json::Value &config);

	int  StartPlaying(void);
	int  Process(void);
	int  Stop(void);

	void Dump(void);

	Json::Value GetConfig(void);

  private:
    std::string m_command;
    std::vector<std::string> m_args;
    std::unique_ptr<Command::Result> m_result;
};

#endif

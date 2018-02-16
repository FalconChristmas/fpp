/*
 *   Falcon Player (FPP) header file
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
 
#ifndef _FPP_H
#define _FPP_H

#define FPP_STATUS_IDLE                 0
#define FPP_STATUS_PLAYLIST_PLAYING     1
#define FPP_STATUS_STOPPING_GRACEFULLY  2
#define FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP  3
#define FPP_STATUS_STOPPING_NOW                    4

#define FPP_SERVER_SOCKET  "/tmp/FPPD"
#define FPP_CLIENT_SOCKET  "/tmp/FPP"

extern int FPPstatus;

#endif

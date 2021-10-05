/*
 *   OverrideZeroOutputProcessor class for Falcon Player (FPP)
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
#include "fpp-pch.h"

#include "OverrideZeroOutputProcessor.h"

OverrideZeroOutputProcessor::OverrideZeroOutputProcessor(const Json::Value& config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    value = config["value"].asInt();
    LogInfo(VB_CHANNELOUT, "Override Zero Channel Range:   %d-%d Value: %d\n",
            start, start + count - 1, value);

    //channel numbers need to be 0 based
    --start;
}

OverrideZeroOutputProcessor::~OverrideZeroOutputProcessor() {
}

void OverrideZeroOutputProcessor::ProcessData(unsigned char* channelData) const {
    for (int x = 0; x < count; x++) {
        if (channelData[x + start] == 0) {
            channelData[x + start] = value;
        }
    }
}

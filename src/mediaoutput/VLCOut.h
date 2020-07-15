#pragma once
/*
 *   libvlc player driver for Falcon Player (FPP)
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

#include "MediaOutputBase.h"

class VLCInternalData;

class VLCOutput : public MediaOutputBase {
  public:
    VLCOutput(const std::string &mediaFilename, MediaOutputStatus *status, const std::string &videoOut);
    virtual ~VLCOutput();

    virtual int Start(int msTime = 0) override;
    virtual int Stop(void) override;
    virtual int Process(void) override;
    virtual int Close(void) override;
    virtual int IsPlaying(void) override;
    
    virtual int Restart();
        
    virtual int AdjustSpeed(float master) override;
    
    
    void SetVolumeAdjustment(int volAdj);

    
    //events from VLC
    virtual void Starting() {}
    virtual void Stopped() {}

  private:
    VLCInternalData *data;
    bool m_allowSpeedAdjust;
};

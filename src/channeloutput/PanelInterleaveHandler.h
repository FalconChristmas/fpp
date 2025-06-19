
#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <cstdint>
#include <string>
#include <vector>

class PanelInterleaveHandler {
public:
    virtual ~PanelInterleaveHandler() {}

    void map(int& x, int& y) {
        int idx = (y * m_panelWidth + x);
        if (idx < 0 || idx >= static_cast<int>(m_map.size())) {
            return; // Out of bounds
        }
        auto& pair = m_map[idx];
        x = pair.first;
        y = pair.second;
    }

    static PanelInterleaveHandler* createHandler(const std::string& type, int panelWidth, int panelHeight, int panelScan);

protected:
    PanelInterleaveHandler(int pw, int ph, int ps, int pi);

    const int m_panelWidth;
    const int m_panelHeight;
    const int m_panelScan;
    const int m_interleave;

    std::vector<std::pair<uint16_t, uint16_t>> m_map;
};

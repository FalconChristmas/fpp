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

    void OverlaySubMatrix(unsigned char* channelData, int i);
    void OverlaySubMatrices(unsigned char* channelData);

private:
    int m_startChannel;
    int m_width;
    int m_height;
    int m_enableFlagOffset;

    unsigned char* m_buffer;

    std::vector<SubMatrix> subMatrix;
};

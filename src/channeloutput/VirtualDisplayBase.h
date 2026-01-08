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

#include <string>
#include <vector>

#include "ChannelOutput.h"

typedef enum virtualPixelColor {
    kVPC_RGB,
    kVPC_RBG,
    kVPC_GRB,
    kVPC_GBR,
    kVPC_BRG,
    kVPC_BGR,
    kVPC_RGBW,
    kVPC_Red,
    kVPC_White,
    kVPC_Blue,
    kVPC_Green,
    kVPC_Custom,
} VirtualPixelColor;

typedef struct virtualDisplayPixel {
    int x;
    int y;
    int z;
    int xs;
    int ys;
    int zs;
    int ch;
    int r;
    int g;
    int b;
    int cpp;
    unsigned char customR;
    unsigned char customG;
    unsigned char customB;

    VirtualPixelColor vpc;
} VirtualDisplayPixel;

class VirtualDisplayBaseOutput : public ChannelOutput {
public:
    VirtualDisplayBaseOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~VirtualDisplayBaseOutput();

    virtual int Init(Json::Value config) override;

    int InitializePixelMap(void);
    int ScaleBackgroundImage(std::string& bgFile, std::string& rgbFile);
    void LoadBackgroundImage(void);

    void GetPixelRGB(VirtualDisplayPixel& pixel, unsigned char* channelData,
                     unsigned char& r, unsigned char& g, unsigned char& b);
    void DrawPixel(int rOffset, int gOffset, int bOffset,
                   unsigned char r, unsigned char g, unsigned char b);
    virtual void DrawPixels(unsigned char* channelData);

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    std::string m_backgroundFilename;
    float m_backgroundBrightness;
    int m_width;
    int m_height;
    int m_bytesPerPixel;
    int m_bpp;
    float m_scale;
    int m_previewWidth;
    int m_previewHeight;
    std::string m_fitTo;
    std::string m_colorOrder;
    unsigned char* m_virtualDisplay;
    int m_pixelSize;

    uint16_t*** m_rgb565map;

    std::vector<VirtualDisplayPixel> m_pixels;
    
protected:
    bool m_allowDuplicatePixels;  // Set to true for 3D mode where multiple pixels can be at same coords
};

inline void VirtualDisplayBaseOutput::GetPixelRGB(VirtualDisplayPixel& pixel,
                                                  unsigned char* channelData, unsigned char& r, unsigned char& g, unsigned char& b) {
    if (pixel.vpc == kVPC_Custom) {
        r = pixel.customR;
        g = pixel.customG;
        b = pixel.customB;
    } else if ((pixel.cpp == 3) ||
               ((pixel.cpp == 4) && (channelData[pixel.ch + 3] == 0))) {
        if (pixel.vpc == kVPC_RGB) {
            r = channelData[pixel.ch];
            g = channelData[pixel.ch + 1];
            b = channelData[pixel.ch + 2];
        } else if (pixel.vpc == kVPC_RBG) {
            r = channelData[pixel.ch];
            g = channelData[pixel.ch + 2];
            b = channelData[pixel.ch + 1];
        } else if (pixel.vpc == kVPC_GRB) {
            r = channelData[pixel.ch + 1];
            g = channelData[pixel.ch];
            b = channelData[pixel.ch + 2];
        } else if (pixel.vpc == kVPC_GBR) {
            r = channelData[pixel.ch + 2];
            g = channelData[pixel.ch];
            b = channelData[pixel.ch + 1];
        } else if (pixel.vpc == kVPC_BRG) {
            r = channelData[pixel.ch + 1];
            g = channelData[pixel.ch + 2];
            b = channelData[pixel.ch];
        } else if (pixel.vpc == kVPC_BGR) {
            r = channelData[pixel.ch + 2];
            g = channelData[pixel.ch + 1];
            b = channelData[pixel.ch];
        }
    } else if (pixel.cpp == 4) {
        r = channelData[pixel.ch + 3];
        g = channelData[pixel.ch + 3];
        b = channelData[pixel.ch + 3];
    } else if (pixel.cpp == 1) {
        if (pixel.vpc == kVPC_Red) {
            r = channelData[pixel.ch];
            g = 0;
            b = 0;
        } else if (pixel.vpc == kVPC_Green) {
            r = 0;
            g = channelData[pixel.ch];
            b = 0;
        } else if (pixel.vpc == kVPC_Blue) {
            r = 0;
            g = 0;
            b = channelData[pixel.ch];
        } else if (pixel.vpc == kVPC_White) {
            r = channelData[pixel.ch];
            g = channelData[pixel.ch];
            b = channelData[pixel.ch];
        }
    }
}

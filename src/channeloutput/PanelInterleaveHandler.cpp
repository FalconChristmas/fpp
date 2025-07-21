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
#include "PanelInterleaveHandler.h"

#include <cstdint>
#include <vector>

PanelInterleaveHandler::PanelInterleaveHandler(int pw, int ph, int ps, int pi) :
    m_panelWidth(pw), m_panelHeight(ph), m_panelScan(ps), m_interleave(pi) {
    m_map.resize(m_panelWidth * m_panelHeight);
}

class NoInterleaveHandler : public PanelInterleaveHandler {
public:
    NoInterleaveHandler(int pw, int ph, int ps, int pi) :
        PanelInterleaveHandler(pw, ph, ps, pi) {
        for (int y = 0; y < m_panelHeight; ++y) {
            for (int x = 0; x < m_panelWidth; ++x) {
                int idx = (y * m_panelWidth + x);
                m_map[idx] = std::make_pair(x, y);
            }
        }
    }
    virtual ~NoInterleaveHandler() {}
};

/*
    For simple interleave of 16: on 32 wide panel
    63----48 31----16

    47----32 15----0
*/
class SimpleInterleaveHandler : public PanelInterleaveHandler {
public:
    SimpleInterleaveHandler(int pw, int ph, int ps, int pi, bool flip) :
        PanelInterleaveHandler(pw, ph, ps, pi),
        m_flipRows(flip) {
        for (int y = 0; y < m_panelHeight; ++y) {
            int y2 = y;
            mapRow(y2);
            for (int x = 0; x < m_panelWidth; ++x) {
                int x2 = x;
                mapCol(y, x2);
                int idx = (y * m_panelWidth + x);
                m_map[idx] = std::make_pair(x2, y2);
            }
        }
    }
    virtual ~SimpleInterleaveHandler() {}

    virtual void mapRow(int& y) {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int& x) {
        int whichInt = x / m_interleave;
        if (m_flipRows) {
            if (y & m_panelScan) {
                y &= !m_panelScan;
            } else {
                y |= m_panelScan;
            }
        }
        int offInInt = x % m_interleave;
        int mult = (m_panelHeight / m_panelScan / 2) - 1 - y / m_panelScan;
        x = m_interleave * (whichInt * m_panelHeight / m_panelScan / 2 + mult) + offInInt;
    }

private:
    const bool m_flipRows;
};

/*
On 32 wide panel:
    63----48 15---0
    47----32 31--16
*/
class ZigZagInterleaveHandler : public PanelInterleaveHandler {
public:
    ZigZagInterleaveHandler(int pw, int ph, int ps, int pi) :
        PanelInterleaveHandler(pw, ph, ps, pi) {
        for (int y = 0; y < m_panelHeight; ++y) {
            int y2 = y;
            mapRow(y2);
            for (int x = 0; x < m_panelWidth; ++x) {
                int x2 = x;
                mapCol(y, x2);
                int idx = (y * m_panelWidth + x);
                m_map[idx] = std::make_pair(x2, y2);
            }
        }
    }
    virtual ~ZigZagInterleaveHandler() {}

    virtual void mapRow(int& y) {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int& x) {
        int whichInt = x / m_interleave;
        int offInInt = x % m_interleave;
        int mult = y / m_panelScan;

        if (m_panelScan == 2) {
            if ((y & 0x2) == 0) {
                offInInt = m_interleave - 1 - offInInt;
            }
        } else if ((whichInt & 0x1) == 1) {
            mult = (y < m_panelScan ? y + m_panelScan : y - m_panelScan) / m_panelScan;
        }
        x = m_interleave * (whichInt * m_panelHeight / m_panelScan / 2 + mult) + offInInt;
    }
};

/*
For 32w panel:
    32----47 0----15
    63----48 31----16
*/
class ZigZagClusterInterleaveHandler : public PanelInterleaveHandler {
public:
    ZigZagClusterInterleaveHandler(int pw, int ph, int ps, int pi) :
        PanelInterleaveHandler(pw, ph, ps, pi) {
        for (int y = 0; y < m_panelHeight; ++y) {
            int y2 = y;
            mapRow(y2);
            for (int x = 0; x < m_panelWidth; ++x) {
                int x2 = x;
                mapCol(y, x2);
                int idx = (y * m_panelWidth + x);
                m_map[idx] = std::make_pair(x2, y2);
            }
        }
    }
    virtual ~ZigZagClusterInterleaveHandler() {}

    virtual void mapRow(int& y) {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int& x) {
        int whichInt = x / m_interleave;
        int offInInt = x % m_interleave;
        int mult = y / m_panelScan;

        if (m_panelScan == 2) {
            // AC_MAPPING code - stealing codepath for scan 1/2 and zigzag8
            // cluster -- the ordinal number of a group of 8 linear lights on a half-frame
            // starting in the top left corner of a half-module; bits 3,2 from y, 1,0 from x
            int tc = whichInt | (y << 1) & 0xc;
            // mapped cluster - this reverses the effects of unusual wiring on this panel
            // address bits are shifted around and bit3 is negated to achieve linear counting
            uint8_t map_cb = (~tc & 8) | (tc & 4) >> 2 | (tc & 2) << 1 | (tc & 1) << 1;
            // scale up from cluster to pixel counts and account for reverse-running clusters
            x = map_cb * 8 + (x & 0x7 ^ (((~y >> 1) & 1) * 7));
            return;
        } else if (m_interleave == 4) {
            if ((whichInt & 0x1) == 1) {
                mult = (y < m_panelScan ? y + m_panelScan : y - m_panelScan) / m_panelScan;
            }
        } else {
            int tmp = (y * 2) / m_panelScan;
            if ((tmp & 0x2) == 0) {
                offInInt = m_interleave - 1 - offInInt;
            }
        }
        x = m_interleave * (whichInt * m_panelHeight / m_panelScan / 2 + mult) + offInInt;
    }
};

class StripeClusterInterleaveHandler : public PanelInterleaveHandler {
public:
    static constexpr int MAPPING[8][4] = {
        { 32, 48, 96, 112 },
        { 32, 48, 96, 112 },
        { 40, 56, 104, 120 },
        { 40, 56, 104, 120 },
        { 0, 16, 64, 80 },
        { 0, 16, 64, 80 },
        { 8, 24, 72, 88 },
        { 8, 24, 72, 88 }
    };

    StripeClusterInterleaveHandler(int pw, int ph, int ps, int pi) :
        PanelInterleaveHandler(pw, ph, ps, pi) {
        for (int y = 0; y < m_panelHeight; ++y) {
            int y2 = y;
            mapRow(y2);
            for (int x = 0; x < m_panelWidth; ++x) {
                int x2 = x;
                mapCol(y, x2);
                int idx = (y * m_panelWidth + x);
                m_map[idx] = std::make_pair(x2, y2);
            }
        }
    }
    virtual ~StripeClusterInterleaveHandler() {}

    virtual void mapRow(int& y) {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int& x) {
        int whichInt = x / m_interleave;
        int offInInt = x % m_interleave;
        x = MAPPING[y % 8][whichInt % 4] + offInInt;
    }
};

class PixelMapper {
public:
    virtual ~PixelMapper() {}

    // Get the name of this PixelMapper. Each PixelMapper needs to have a name
    // so that it can be referred to with command line flags.
    virtual const char* GetName() const = 0;

    // Pixel mappers receive the chain and parallel information and
    // might receive optional user-parameters, e.g. from command line flags.
    //
    // This is a single string containing the parameters.
    // You can be used from simple scalar parameters, such as the angle for
    // the rotate transformer, or more complex parameters that describe a mapping
    // of panels for instance.
    // Keep it concise (as people will give parameters on the command line) and
    // don't use semicolons in your string (as they are
    // used to separate pixel mappers on the command line).
    //
    // For instance, the rotate transformer is invoked like this
    //  --led-pixel-mapper=rotate:90
    // And the parameter that is passed to SetParameter() is "90".
    //
    // Returns 'true' if parameter was parsed successfully.
    virtual bool SetParameters(int chain, int parallel,
                               const char* parameter_string) {
        return true;
    }

    // Given a underlying matrix (width, height), returns the
    // visible (width, height) after the mapping.
    // E.g. a 90 degree rotation might map matrix=(64, 32) -> visible=(32, 64)
    // Some multiplexing matrices will double the height and half the width.
    //
    // While not technically necessary, one would expect that the number of
    // pixels stay the same, so
    // matrix_width * matrix_height == (*visible_width) * (*visible_height);
    //
    // Returns boolean "true" if the mapping can be successfully done with this
    // mapper.
    virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                                int* visible_width, int* visible_height)
        const = 0;

    // Map where a visible pixel (x,y) is mapped to the underlying matrix (x,y).
    //
    // To be convienently stateless, the first parameters are the full
    // matrix width and height.
    //
    // So for many multiplexing methods this means to map a panel to a double
    // length and half height panel (32x16 -> 64x8).
    // The logic_x, logic_y are output parameters and guaranteed not to be
    // nullptr.
    virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                    int visible_x, int visible_y,
                                    int* matrix_x, int* matrix_y) const = 0;
};
class MultiplexMapper : public PixelMapper {
public:
    // Function that edits the original rows and columns of the panels
    // provided by the user to the actual underlying mapping. This is called
    // before we do the actual set-up of the GPIO mapping as this influences
    // the hardware interface.
    // This is so that the user can provide the rows/columns they see.
    virtual void EditColsRows(int* cols, int* rows) const = 0;
};

class MultiplexMapperBase : public MultiplexMapper {
public:
    MultiplexMapperBase(const char* name, int stretch_factor) :
        name_(name), panel_stretch_factor_(stretch_factor) {}

    // This method is const, but we sneakily remember the original size
    // of the panels so that we can more easily quantize things.
    // So technically, we're stateful, but let's pretend we're not changing
    // state. In the context this is used, it is never accessed in multiple
    // threads.
    virtual void EditColsRows(int* cols, int* rows) const {
        panel_rows_ = *rows;
        panel_cols_ = *cols;

        *rows /= panel_stretch_factor_;
        *cols *= panel_stretch_factor_;
    }

    virtual bool GetSizeMapping(int matrix_width, int matrix_height,
                                int* visible_width, int* visible_height) const {
        // Matrix width has been altered. Alter it back.
        *visible_width = matrix_width / panel_stretch_factor_;
        *visible_height = matrix_height * panel_stretch_factor_;
        return true;
    }

    virtual const char* GetName() const { return name_; }

    // The MapVisibleToMatrix() as required by PanelMatrix here breaks it
    // down to the individual panel, so that derived classes only need to
    // implement MapSinglePanel().
    virtual void MapVisibleToMatrix(int matrix_width, int matrix_height,
                                    int visible_x, int visible_y,
                                    int* matrix_x, int* matrix_y) const {
        const int chained_panel = visible_x / panel_cols_;
        const int parallel_panel = visible_y / panel_rows_;

        const int within_panel_x = visible_x % panel_cols_;
        const int within_panel_y = visible_y % panel_rows_;

        int new_x, new_y;
        MapSinglePanel(within_panel_x, within_panel_y, &new_x, &new_y);
        *matrix_x = chained_panel * panel_stretch_factor_ * panel_cols_ + new_x;
        *matrix_y = parallel_panel * panel_rows_ / panel_stretch_factor_ + new_y;
    }

    // Map the coordinates for a single panel. This is to be overridden in
    // derived classes.
    // Input parameter is the visible position on the matrix, and this method
    // should return the internal multiplexed position.
    virtual void MapSinglePanel(int visible_x, int visible_y,
                                int* matrix_x, int* matrix_y) const = 0;

protected:
    const char* const name_;
    const int panel_stretch_factor_;

    mutable int panel_cols_;
    mutable int panel_rows_;
};

/* ========================================================================
 * Multiplexer implementations.
 *
 * Extend MultiplexMapperBase and implement MapSinglePanel. You only have
 * to worry about the mapping within a single panel, the overall panel
 * construction with chains and parallel is already taken care of.
 *
 * Don't forget to register the new multiplexer sin CreateMultiplexMapperList()
 * below. After that, the new mapper is available in the --led-multiplexing
 * option.
 */
class StripeMultiplexMapper : public MultiplexMapperBase {
public:
    StripeMultiplexMapper() :
        MultiplexMapperBase("Stripe", 2) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        const bool is_top_stripe = (y % (panel_rows_ / 2)) < panel_rows_ / 4;
        *matrix_x = is_top_stripe ? x + panel_cols_ : x;
        *matrix_y = ((y / (panel_rows_ / 2)) * (panel_rows_ / 4) + y % (panel_rows_ / 4));
    }
};

class FlippedStripeMultiplexMapper : public MultiplexMapperBase {
public:
    FlippedStripeMultiplexMapper() :
        MultiplexMapperBase("FlippedStripe", 2) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        const bool is_top_stripe = (y % (panel_rows_ / 2)) >= panel_rows_ / 4;
        *matrix_x = is_top_stripe ? x + panel_cols_ : x;
        *matrix_y = ((y / (panel_rows_ / 2)) * (panel_rows_ / 4) + y % (panel_rows_ / 4));
    }
};

class CheckeredMultiplexMapper : public MultiplexMapperBase {
public:
    CheckeredMultiplexMapper() :
        MultiplexMapperBase("Checkered", 2) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        const bool is_top_check = (y % (panel_rows_ / 2)) < panel_rows_ / 4;
        const bool is_left_check = (x < panel_cols_ / 2);
        if (is_top_check) {
            *matrix_x = is_left_check ? x + panel_cols_ / 2 : x + panel_cols_;
        } else {
            *matrix_x = is_left_check ? x : x + panel_cols_ / 2;
        }
        *matrix_y = ((y / (panel_rows_ / 2)) * (panel_rows_ / 4) + y % (panel_rows_ / 4));
    }
};

class SpiralMultiplexMapper : public MultiplexMapperBase {
public:
    SpiralMultiplexMapper() :
        MultiplexMapperBase("Spiral", 2) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        const bool is_top_stripe = (y % (panel_rows_ / 2)) < panel_rows_ / 4;
        const int panel_quarter = panel_cols_ / 4;
        const int quarter = x / panel_quarter;
        const int offset = x % panel_quarter;
        *matrix_x = ((2 * quarter * panel_quarter) + (is_top_stripe
                                                          ? panel_quarter - 1 - offset
                                                          : panel_quarter + offset));
        *matrix_y = ((y / (panel_rows_ / 2)) * (panel_rows_ / 4) + y % (panel_rows_ / 4));
    }
};

class ZStripeMultiplexMapper : public MultiplexMapperBase {
public:
    ZStripeMultiplexMapper(const char* name, int even_vblock_offset, int odd_vblock_offset) :
        MultiplexMapperBase(name, 2),
        even_vblock_offset_(even_vblock_offset),
        odd_vblock_offset_(odd_vblock_offset) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        static const int tile_width = 8;
        static const int tile_height = 4;

        const int vert_block_is_odd = ((y / tile_height) % 2);

        const int even_vblock_shift = (1 - vert_block_is_odd) * even_vblock_offset_;
        const int odd_vblock_shitf = vert_block_is_odd * odd_vblock_offset_;

        *matrix_x = x + ((x + even_vblock_shift) / tile_width) * tile_width + odd_vblock_shitf;
        *matrix_y = (y % tile_height) + tile_height * (y / (tile_height * 2));
    }

private:
    const int even_vblock_offset_;
    const int odd_vblock_offset_;
};

class CoremanMapper : public MultiplexMapperBase {
public:
    CoremanMapper() :
        MultiplexMapperBase("coreman", 2) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        const bool is_left_check = (x < panel_cols_ / 2);

        if ((y <= 7) || ((y >= 16) && (y <= 23))) {
            *matrix_x = ((x / (panel_cols_ / 2)) * panel_cols_) + (x % (panel_cols_ / 2));
            if ((y & (panel_rows_ / 4)) == 0) {
                *matrix_y = (y / (panel_rows_ / 2)) * (panel_rows_ / 4) + (y % (panel_rows_ / 4));
            }
        } else {
            *matrix_x = is_left_check ? x + panel_cols_ / 2 : x + panel_cols_;
            *matrix_y = (y / (panel_rows_ / 2)) * (panel_rows_ / 4) + y % (panel_rows_ / 4);
        }
    }
};

class Kaler2ScanMapper : public MultiplexMapperBase {
public:
    Kaler2ScanMapper() :
        MultiplexMapperBase("Kaler2Scan", 4) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        // Now we have a 128x4 matrix
        int offset = ((y % 4) / 2) == 0 ? -1 : 1; // Add o substract
        int deltaOffset = offset < 0 ? 7 : 8;
        int deltaColumn = ((y % 8) / 4) == 0 ? 64 : 0;

        *matrix_y = (y % 2 + (y / 8) * 2);
        *matrix_x = deltaColumn + (16 * (x / 8)) + deltaOffset + ((x % 8) * offset);
    }
};

class P10MapperZ : public MultiplexMapperBase {
public:
    P10MapperZ() :
        MultiplexMapperBase("P10-128x4-Z", 4) {}
    // supports this panel: https://www.aliexpress.com/item/2017-Special-Offer-P10-Outdoor-Smd-Full-Color-Led-Display-Module-320x160mm-1-2-Scan-Outdoor/32809267439.html?spm=a2g0s.9042311.0.0.Ob0jEw
    // with --led-row-addr-type=2 flag
    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        int yComp = 0;
        if (y == 0 || y == 1 || y == 8 || y == 9) {
            yComp = 127;
        } else if (y == 2 || y == 3 || y == 10 || y == 11) {
            yComp = 112;
        } else if (y == 4 || y == 5 || y == 12 || y == 13) {
            yComp = 111;
        } else if (y == 6 || y == 7 || y == 14 || y == 15) {
            yComp = 96;
        }

        if (y == 0 || y == 1 || y == 4 || y == 5 ||
            y == 8 || y == 9 || y == 12 || y == 13) {
            *matrix_x = yComp - x;
            *matrix_x -= (24 * ((int)(x / 8)));
        } else {
            *matrix_x = yComp + x;
            *matrix_x -= (40 * ((int)(x / 8)));
        }

        if (y == 0 || y == 2 || y == 4 || y == 6) {
            *matrix_y = 3;
        } else if (y == 1 || y == 3 || y == 5 || y == 7) {
            *matrix_y = 2;
        } else if (y == 8 || y == 10 || y == 12 || y == 14) {
            *matrix_y = 1;
        } else if (y == 9 || y == 11 || y == 13 || y == 15) {
            *matrix_y = 0;
        }
    }
};

class QiangLiQ8 : public MultiplexMapperBase {
public:
    QiangLiQ8() :
        MultiplexMapperBase("QiangLiQ8", 2) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        const int column = x + (4 + 4 * (x / 4));
        *matrix_x = column;
        if ((y >= 15 && y <= 19) || (y >= 5 && y <= 9)) {
            const int reverseColumn = x + (4 * (x / 4));
            *matrix_x = reverseColumn;
        }
        *matrix_y = y % 5 + (y / 10) * 5;
    }
};

class InversedZStripe : public MultiplexMapperBase {
public:
    InversedZStripe() :
        MultiplexMapperBase("InversedZStripe", 2) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        static const int tile_width = 8;
        static const int tile_height = 4;

        const int vert_block_is_odd = ((y / tile_height) % 2);
        const int evenOffset[8] = { 7, 5, 3, 1, -1, -3, -5, -7 };

        if (vert_block_is_odd) {
            *matrix_x = x + (x / tile_width) * tile_width;
        } else {
            *matrix_x = x + (x / tile_width) * tile_width + 8 + evenOffset[x % 8];
        }
        *matrix_y = (y % tile_height) + tile_height * (y / (tile_height * 2));
    }
};

/*
 * Vairous P10 1R1G1B Outdoor implementations for 16x16 modules with separate
 * RGB LEDs, e.g.:
 * https://www.ledcontrollercard.com/english/p10-outdoor-rgb-led-module-160x160mm-dip.html
 *
 */
class P10Outdoor1R1G1BMultiplexBase : public MultiplexMapperBase {
public:
    P10Outdoor1R1G1BMultiplexBase(const char* name) :
        MultiplexMapperBase(name, 2) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        const int vblock_is_odd = (y / tile_height_) % 2;
        const int vblock_is_even = 1 - vblock_is_odd;
        const int even_vblock_shift = vblock_is_even * even_vblock_offset_;
        const int odd_vblock_shift = vblock_is_odd * odd_vblock_offset_;

        MapPanel(x, y, matrix_x, matrix_y,
                 vblock_is_even, vblock_is_odd,
                 even_vblock_shift, odd_vblock_shift);
    }

protected:
    virtual void MapPanel(int x, int y, int* matrix_x, int* matrix_y,
                          int vblock_is_even, int vblock_is_odd,
                          int even_vblock_shift, int odd_vblock_shift) const = 0;

    static const int tile_width_ = 8;
    static const int tile_height_ = 4;
    static const int even_vblock_offset_ = 0;
    static const int odd_vblock_offset_ = 8;
};

class P10Outdoor1R1G1BMultiplexMapper1 : public P10Outdoor1R1G1BMultiplexBase {
public:
    P10Outdoor1R1G1BMultiplexMapper1() :
        P10Outdoor1R1G1BMultiplexBase("P10Outdoor1R1G1-1") {}

protected:
    void MapPanel(int x, int y, int* matrix_x, int* matrix_y,
                  const int vblock_is_even, const int vblock_is_odd,
                  const int even_vblock_shift, const int odd_vblock_shift) const {
        *matrix_x = tile_width_ * (1 + vblock_is_even + 2 * (x / tile_width_)) - (x % tile_width_) - 1;
        *matrix_y = (y % tile_height_) + tile_height_ * (y / (tile_height_ * 2));
    }
};

class P10Outdoor1R1G1BMultiplexMapper2 : public P10Outdoor1R1G1BMultiplexBase {
public:
    P10Outdoor1R1G1BMultiplexMapper2() :
        P10Outdoor1R1G1BMultiplexBase("P10Outdoor1R1G1-2") {}

protected:
    void MapPanel(int x, int y, int* matrix_x, int* matrix_y,
                  const int vblock_is_even, const int vblock_is_odd,
                  const int even_vblock_shift, const int odd_vblock_shift) const {
        *matrix_x = vblock_is_even
                        ? tile_width_ * (1 + 2 * (x / tile_width_)) - (x % tile_width_) - 1
                        : x + ((x + even_vblock_shift) / tile_width_) * tile_width_ + odd_vblock_shift;
        *matrix_y = (y % tile_height_) + tile_height_ * (y / (tile_height_ * 2));
    }
};

class P10Outdoor1R1G1BMultiplexMapper3 : public P10Outdoor1R1G1BMultiplexBase {
public:
    P10Outdoor1R1G1BMultiplexMapper3() :
        P10Outdoor1R1G1BMultiplexBase("P10Outdoor1R1G1-3") {}

protected:
    void MapPanel(int x, int y, int* matrix_x, int* matrix_y,
                  const int vblock_is_even, const int vblock_is_odd,
                  const int even_vblock_shift, const int odd_vblock_shift) const {
        *matrix_x = vblock_is_odd
                        ? tile_width_ * (2 + 2 * (x / tile_width_)) - (x % tile_width_) - 1
                        : x + ((x + even_vblock_shift) / tile_width_) * tile_width_ + odd_vblock_shift;
        *matrix_y = (y % tile_height_) + tile_height_ * (y / (tile_height_ * 2));
    }
};

class P10CoremanMapper : public MultiplexMapperBase {
public:
    P10CoremanMapper() :
        MultiplexMapperBase("P10CoremanMapper", 4) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        // Row offset 8,8,8,8,0,0,0,0,8,8,8,8,0,0,0,0
        int mulY = (y & 4) > 0 ? 0 : 8;

        // Row offset 9,9,8,8,1,1,0,0,9,9,8,8,1,1,0,0
        mulY += (y & 2) > 0 ? 0 : 1;
        mulY += (x >> 2) & ~1; // Drop lsb

        *matrix_x = (mulY << 3) + x % 8;
        *matrix_y = (y & 1) + ((y >> 2) & ~1);
    }
};

/*
 * P8 1R1G1B Outdoor P8-5S-V3.2-HX 20x40
 */
class P8Outdoor1R1G1BMultiplexBase : public MultiplexMapperBase {
public:
    P8Outdoor1R1G1BMultiplexBase(const char* name) :
        MultiplexMapperBase(name, 2) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        const int vblock_is_odd = (y / tile_height_) % 2;
        const int vblock_is_even = 1 - vblock_is_odd;
        const int even_vblock_shift = vblock_is_even * even_vblock_offset_;
        const int odd_vblock_shift = vblock_is_odd * odd_vblock_offset_;

        MapPanel(x, y, matrix_x, matrix_y,
                 vblock_is_even, vblock_is_odd,
                 even_vblock_shift, odd_vblock_shift);
    }

protected:
    virtual void MapPanel(int x, int y, int* matrix_x, int* matrix_y,
                          int vblock_is_even, int vblock_is_odd,
                          int even_vblock_shift, int odd_vblock_shift) const = 0;

    static const int tile_width_ = 8;
    static const int tile_height_ = 5;
    static const int even_vblock_offset_ = 0;
    static const int odd_vblock_offset_ = 8;
};

class P8Outdoor1R1G1BMultiplexMapper : public P8Outdoor1R1G1BMultiplexBase {
public:
    P8Outdoor1R1G1BMultiplexMapper() :
        P8Outdoor1R1G1BMultiplexBase("P8Outdoor1R1G1") {}

protected:
    void MapPanel(int x, int y, int* matrix_x, int* matrix_y,
                  const int vblock_is_even, const int vblock_is_odd,
                  const int even_vblock_shift, const int odd_vblock_shift) const {
        *matrix_x = vblock_is_even
                        ? tile_width_ * (1 + tile_width_ - 2 * (x / tile_width_)) + tile_width_ - (x % tile_width_) - 1
                        : tile_width_ * (1 + tile_width_ - 2 * (x / tile_width_)) - tile_width_ + (x % tile_width_);

        *matrix_y = (tile_height_ - y % tile_height_) + tile_height_ * (1 - y / (tile_height_ * 2)) - 1;
    }
};

class P10Outdoor32x16HalfScanMapper : public MultiplexMapperBase {
public:
    P10Outdoor32x16HalfScanMapper() :
        MultiplexMapperBase("P10Outdoor32x16HalfScan", 4) {}

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        int base = (x / 8) * 32;
        bool reverse = (y % 4) / 2 == 0;
        int offset = (3 - ((y % 8) / 2)) * 8;
        int dx = x % 8;

        *matrix_y = (y / 8 == 0) ? ((y % 2 == 0) ? 0 : 1) : ((y % 2 == 0) ? 2 : 3);
        *matrix_x = base + (reverse ? offset + (7 - dx) : offset + dx);
    }
};

class P10Outdoor32x16QuarterScanMapper : public MultiplexMapperBase {
public:
    P10Outdoor32x16QuarterScanMapper() :
        MultiplexMapperBase("P10Outdoor32x16QuarterScanMapper", 4) {}
    // P10 quarter scan panel, e.g. https://www.ebay.com.au/itm/175517677191

    void EditColsRows(int* cols, int* rows) const {
        panel_rows_ = *rows;
        panel_cols_ = *cols;

        *rows /= panel_stretch_factor_ / 2; // has half stretch factor in y compared to x
        *cols *= panel_stretch_factor_;
    }

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        int cell_starting_point = (x / 8) * 32;
        int delta_x = x % 8;
        int offset = (3 - (y / 4)) * 8;
        *matrix_x = cell_starting_point + delta_x + offset;
        *matrix_y = y % 4;
    }
};

class P3Outdoor64x64MultiplexMapper : public MultiplexMapperBase {
public:
    P3Outdoor64x64MultiplexMapper() :
        MultiplexMapperBase("P3Outdoor64x64MultiplexMapper", 2) {}
    // P3 RGB panel 64x64
    // with pattern   [1] [3]
    //                 | \ |
    //                [0] [2]

    void MapSinglePanel(int x, int y, int* matrix_x, int* matrix_y) const {
        const bool is_top_stripe = (y % (panel_rows_ / 2)) < panel_rows_ / 4;
        *matrix_x = ((x * 2) + (is_top_stripe ? 1 : 0));
        *matrix_y = ((y / (panel_rows_ / 2)) * (panel_rows_ / 4) + y % (panel_rows_ / 4));
    }
};

class PiRGBInterleaveHandler : public PanelInterleaveHandler {
public:
    PiRGBInterleaveHandler(int pw, int ph, int ps, int pi) :
        PanelInterleaveHandler(pw, ph, ps, 8) {
        switch (pi) {
        case 1:
            base = new StripeMultiplexMapper();
            break;
        case 2:
            base = new CheckeredMultiplexMapper();
            break;
        case 3:
            base = new SpiralMultiplexMapper();
            break;
        case 4:
            base = new ZStripeMultiplexMapper("ZStripe", 0, 8);
            break;
        case 5:
            base = new ZStripeMultiplexMapper("ZnMirrorZStripe", 4, 4);
            break;
        case 6:
            base = new CoremanMapper();
            break;
        case 7:
            base = new Kaler2ScanMapper();
            break;
        case 8:
            base = new ZStripeMultiplexMapper("ZStripeUneven", 8, 0);
            break;
        case 9:
            base = new P10MapperZ();
            break;
        case 10:
            base = new QiangLiQ8();
            break;
        case 11:
            base = new InversedZStripe();
            break;
        case 12:
            base = new P10Outdoor1R1G1BMultiplexMapper1();
            break;
        case 13:
            base = new P10Outdoor1R1G1BMultiplexMapper2();
            break;
        case 14:
            base = new P10Outdoor1R1G1BMultiplexMapper3();
            break;
        case 15:
            base = new P10CoremanMapper();
            break;
        case 16:
            base = new P8Outdoor1R1G1BMultiplexMapper();
            break;
        case 17:
            base = new FlippedStripeMultiplexMapper();
            break;
        case 18:
            base = new P10Outdoor32x16HalfScanMapper();
            break;
        case 19:
            base = new P10Outdoor32x16QuarterScanMapper();
            break;
        case 20:
            base = new P3Outdoor64x64MultiplexMapper();
            break;
        }
        base->EditColsRows(&pw, &ph);
        for (int y = 0; y < m_panelHeight; ++y) {
            for (int x = 0; x < m_panelWidth; ++x) {
                int idx = (y * m_panelWidth + x);

                int x1 = x;
                int y1 = y;
                base->MapVisibleToMatrix(m_panelWidth, m_panelHeight, x1, y1, &x1, &y1);
                m_map[idx] = std::make_pair(x1, y1);
            }
        }
    }
    virtual ~PiRGBInterleaveHandler() {}

    MultiplexMapperBase* base;
};

PanelInterleaveHandler* PanelInterleaveHandler::createHandler(const std::string& type, int panelWidth, int panelHeight, int panelScan) {
    if ((panelScan * 2) == panelHeight) {
        return new NoInterleaveHandler(panelWidth, panelHeight, panelScan, 0);
    }
    if (type == "4z") {
        return new ZigZagInterleaveHandler(panelWidth, panelHeight, panelScan, 4);
    } else if (type == "8z") {
        return new ZigZagInterleaveHandler(panelWidth, panelHeight, panelScan, 8);
    } else if (type == "16z") {
        return new ZigZagInterleaveHandler(panelWidth, panelHeight, panelScan, 16);
    } else if (type == "32z") {
        return new ZigZagInterleaveHandler(panelWidth, panelHeight, panelScan, 32);
    } else if (type == "40z") {
        return new ZigZagInterleaveHandler(panelWidth, panelHeight, panelScan, 40);
    } else if (type == "1f") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 1, true);
    } else if (type == "2f") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 2, true);
    } else if (type == "4f") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 4, true);
    } else if (type == "8f") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 8, true);
    } else if (type == "16f") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 16, true);
    } else if (type == "32f") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 32, true);
    } else if (type == "40f") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 32, true);
    } else if (type == "64f") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 64, true);
    } else if (type == "80f") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 80, true);
    } else if (type == "1") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 1, false);
    } else if (type == "2") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 2, false);
    } else if (type == "4") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 4, false);
    } else if (type == "8") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 8, false);
    } else if (type == "16") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 16, false);
    } else if (type == "32") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 32, false);
    } else if (type == "40") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 40, false);
    } else if (type == "64") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 64, false);
    } else if (type == "80") {
        return new SimpleInterleaveHandler(panelWidth, panelHeight, panelScan, 80, false);
    } else if (type == "8c") {
        return new ZigZagClusterInterleaveHandler(panelWidth, panelHeight, panelScan, 8);
    } else if (type == "16c") {
        return new ZigZagClusterInterleaveHandler(panelWidth, panelHeight, panelScan, 16);
    } else if (type == "8s") {
        return new StripeClusterInterleaveHandler(panelWidth, panelHeight, panelScan, 8);
    } else if (type == "16s") {
        return new StripeClusterInterleaveHandler(panelWidth, panelHeight, panelScan, 16);
    } else if (type.starts_with("RPi")) {
        std::string idx = type.substr(3);
        int idxValue = std::atoi(idx.c_str());
        return new PiRGBInterleaveHandler(panelWidth, panelHeight, panelScan, idxValue);
    }

    return new NoInterleaveHandler(panelWidth, panelHeight, panelScan, 0);
}

#ifdef STANDALONE

int main(int argc, char* argv[]) {
    // Example usage of PanelInterleaveHandler
    int panelWidth = 32;
    int panelHeight = 32;
    int panelScan = 8;

    PanelInterleaveHandler* handler = PanelInterleaveHandler::createHandler(argv[1], panelWidth, panelHeight, panelScan);

    for (int x = 0; x < panelWidth; ++x) {
        int xOut = x;
        int yOut = 0;
        handler->map(xOut, yOut);
        printf("%d %d -> %d %d       ", x, 0, xOut, yOut);
        xOut = x;
        yOut = 8;
        handler->map(xOut, yOut);
        printf("%d %d -> %d %d\n", x, 8, xOut, yOut);
    }
    int x = 5, y = 3;
    handler->map(x, y);

    delete handler;
    return 0;
}

#endif

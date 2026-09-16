/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <algorithm>
#include <cmath>

#include "TextColorizer.h"

namespace TextColor {

namespace {

/** Phase is quantised to this many steps before seeding the sparkle noise, so
 *  flecks hold for a frame or two instead of boiling at the repaint rate. */
constexpr uint32_t SPARKLE_STEPS = 12;
/** Coverage above which a sparkle pixel is blown out to white. */
constexpr double SPARKLE_WHITE_AT = 0.93;
/** Dimmest a non-flecked sparkle pixel gets, as a fraction of its colour. */
constexpr double SPARKLE_FLOOR = 0.35;
/** How far a random-per-letter mode re-rolls its colours over one phase cycle. */
constexpr uint32_t RANDOM_STEPS = 8;
/** Width of the Chase highlight as a fraction of the text, as a gaussian sigma. */
constexpr double CHASE_SIGMA = 0.07;
/** Brightness of the text outside the Chase highlight. */
constexpr double CHASE_FLOOR = 0.2;

inline double fract(double v) {
    return v - std::floor(v);
}

inline uint32_t hash32(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = a * 0x9E3779B1u;
    h ^= b + 0x85EBCA6Bu + (h << 6) + (h >> 2);
    h ^= c + 0xC2B2AE35u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return h;
}

/** hash32 folded down to [0, 1). */
inline double hashUnit(uint32_t a, uint32_t b, uint32_t c) {
    return (double)(hash32(a, b, c) & 0xFFFFFF) / (double)0x1000000;
}

inline uint32_t packRGB(int r, int g, int b) {
    return ((uint32_t)std::clamp(r, 0, 255) << 16) |
           ((uint32_t)std::clamp(g, 0, 255) << 8) |
           (uint32_t)std::clamp(b, 0, 255);
}

/** Full-saturation, full-value hue sweep. h is wrapped into [0, 1). */
uint32_t hueColor(double h) {
    h = fract(h) * 6.0;
    int sector = (int)h;
    double f = h - sector;
    int v = 255;
    int p = 0;
    int q = (int)std::lround(255.0 * (1.0 - f));
    int t = (int)std::lround(255.0 * f);
    switch (sector) {
    case 0:
        return packRGB(v, t, p);
    case 1:
        return packRGB(q, v, p);
    case 2:
        return packRGB(p, v, t);
    case 3:
        return packRGB(p, q, v);
    case 4:
        return packRGB(t, p, v);
    default:
        return packRGB(v, p, q);
    }
}

uint32_t lerpColor(uint32_t a, uint32_t b, double f) {
    f = std::clamp(f, 0.0, 1.0);
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    return packRGB((int)std::lround(ar + (br - ar) * f),
                   (int)std::lround(ag + (bg - ag) * f),
                   (int)std::lround(ab + (bb - ab) * f));
}

/**
 * Sample the palette as a continuous ramp.
 *
 * `cyclic` places the stops at i/N with a wrap from the last back to the
 * first, which is what an animated ramp needs to avoid a hard seam sliding
 * through the text once per cycle. A still ramp instead places them at i/(N-1)
 * so a two-colour gradient reads exactly as asked: first colour at one end,
 * second at the other.
 */
uint32_t rampColor(const Palette& pal, double t, bool cyclic) {
    if (pal.rainbow) {
        return hueColor(t);
    }
    size_t n = pal.colors.size();
    if (n == 0) {
        return 0xFFFFFF;
    }
    if (n == 1) {
        return pal.colors[0];
    }
    if (cyclic) {
        double pos = fract(t) * (double)n;
        size_t i = (size_t)pos;
        if (i >= n) {
            i = n - 1;
        }
        return lerpColor(pal.colors[i], pal.colors[(i + 1) % n], pos - (double)i);
    }
    double pos = std::clamp(t, 0.0, 1.0) * (double)(n - 1);
    size_t i = (size_t)pos;
    if (i >= n - 1) {
        return pal.colors[n - 1];
    }
    return lerpColor(pal.colors[i], pal.colors[i + 1], pos - (double)i);
}

/**
 * Pick a flat colour for span `index` of `count`. A rainbow palette spreads
 * evenly over the spans so e.g. per-letter text reads as one clean sweep
 * rather than an arbitrary slice of the hue circle; a listed palette simply
 * cycles, advancing by `count` positions over a full phase cycle so animation
 * chases the colours along the text.
 */
uint32_t spanColor(const Palette& pal, int index, int count, double phase) {
    if (index < 0) {
        index = 0;
    }
    if (count < 1) {
        count = 1;
    }
    if (pal.rainbow) {
        return hueColor((double)index / (double)count + phase);
    }
    size_t n = pal.colors.size();
    if (n == 0) {
        return 0xFFFFFF;
    }
    uint32_t shift = (uint32_t)(phase * (double)n);
    return pal.colors[((uint32_t)index + shift) % n];
}

/** As spanColor, but the assignment is scrambled rather than sequential. */
uint32_t randomSpanColor(const Palette& pal, int index, double phase, uint32_t seed) {
    if (index < 0) {
        index = 0;
    }
    uint32_t bucket = (uint32_t)(phase * (double)RANDOM_STEPS);
    uint32_t h = hash32(seed, (uint32_t)index, bucket);
    if (pal.rainbow) {
        return hueColor((double)(h & 0xFFFFFF) / (double)0x1000000);
    }
    size_t n = pal.colors.size();
    if (n == 0) {
        return 0xFFFFFF;
    }
    return pal.colors[h % n];
}

uint32_t scaleColor(uint32_t c, double f) {
    if (f >= 1.0) {
        return c;
    }
    if (f <= 0.0) {
        return 0;
    }
    return packRGB((int)std::lround(((c >> 16) & 0xFF) * f),
                   (int)std::lround(((c >> 8) & 0xFF) * f),
                   (int)std::lround((c & 0xFF) * f));
}

} // namespace

const std::vector<std::string>& ModeNames() {
    static const std::vector<std::string> NAMES = {
        "Single",
        "Gradient Horizontal",
        "Gradient Vertical",
        "Gradient Diagonal",
        "Per Letter",
        "Per Word",
        "Per Line",
        "Random Letter",
        "Random Word",
        "Sparkle",
        "Chase"
    };
    return NAMES;
}

Mode ParseMode(const std::string& name) {
    if (name == "Gradient Horizontal") {
        return Mode::GradientHorizontal;
    } else if (name == "Gradient Vertical") {
        return Mode::GradientVertical;
    } else if (name == "Gradient Diagonal") {
        return Mode::GradientDiagonal;
    } else if (name == "Per Letter") {
        return Mode::PerLetter;
    } else if (name == "Per Word") {
        return Mode::PerWord;
    } else if (name == "Per Line") {
        return Mode::PerLine;
    } else if (name == "Random Letter") {
        return Mode::RandomLetter;
    } else if (name == "Random Word") {
        return Mode::RandomWord;
    } else if (name == "Sparkle") {
        return Mode::Sparkle;
    } else if (name == "Chase") {
        return Mode::Chase;
    }
    return Mode::Single;
}

bool NeedsLetterSpans(Mode m) {
    return m == Mode::PerLetter || m == Mode::RandomLetter;
}
bool NeedsWordSpans(Mode m) {
    return m == Mode::PerWord || m == Mode::RandomWord;
}
bool NeedsLineSpans(Mode m) {
    return m == Mode::PerLine;
}
bool IsAnimated(Mode m) {
    return m != Mode::Single;
}

const std::vector<std::string>& PaletteNames() {
    static const std::vector<std::string> NAMES = {
        "Custom",
        "Rainbow",
        "Christmas",
        "Halloween",
        "Winter",
        "Fire",
        "Ocean",
        "Forest",
        "Patriotic",
        "Pastel",
        "Party"
    };
    return NAMES;
}

Palette BuildPalette(const std::string& name, const std::vector<uint32_t>& custom) {
    Palette p;
    if (name == "Rainbow") {
        p.rainbow = true;
        return p;
    }
    if (name == "Christmas") {
        p.colors = { 0xFF0000, 0x00FF00, 0xFFFFFF };
    } else if (name == "Halloween") {
        p.colors = { 0xFF6000, 0x8000FF, 0x00FF00 };
    } else if (name == "Winter") {
        p.colors = { 0xFFFFFF, 0x80D0FF, 0x0040FF };
    } else if (name == "Fire") {
        p.colors = { 0xFF0000, 0xFF6000, 0xFFC000, 0xFFFF80 };
    } else if (name == "Ocean") {
        p.colors = { 0x0020FF, 0x00A0FF, 0x00FFC0 };
    } else if (name == "Forest") {
        p.colors = { 0x004000, 0x00A000, 0x80FF00 };
    } else if (name == "Patriotic") {
        p.colors = { 0xFF0000, 0xFFFFFF, 0x0000FF };
    } else if (name == "Pastel") {
        p.colors = { 0xFFB0C0, 0xB0E0FF, 0xFFF0A0, 0xB0FFC0 };
    } else if (name == "Party") {
        p.colors = { 0xFF00A0, 0x00FFFF, 0xFFFF00, 0x80FF00, 0xFF4000 };
    } else {
        // "Custom" and anything unrecognised: the caller's own colours.
        p.colors = custom;
    }
    if (p.colors.empty()) {
        p.colors.push_back(0xFFFFFF);
    }
    return p;
}

bool InkBounds(const uint8_t* mask, int w, int h, Layout& out) {
    if (!mask || w <= 0 || h <= 0) {
        return false;
    }
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = mask + (size_t)y * w;
        for (int x = 0; x < w; ++x) {
            if (row[x]) {
                if (x < minX) {
                    minX = x;
                }
                if (x > maxX) {
                    maxX = x;
                }
                if (y < minY) {
                    minY = y;
                }
                maxY = y;
            }
        }
    }
    if (maxX < 0) {
        return false;
    }
    out.x = minX;
    out.y = minY;
    out.w = maxX - minX + 1;
    out.h = maxY - minY + 1;
    return true;
}

std::vector<std::pair<int, int>> InkRowRuns(const uint8_t* mask, int w, int h) {
    std::vector<std::pair<int, int>> runs;
    if (!mask || w <= 0 || h <= 0) {
        return runs;
    }
    int start = -1;
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = mask + (size_t)y * w;
        bool lit = false;
        for (int x = 0; x < w; ++x) {
            if (row[x]) {
                lit = true;
                break;
            }
        }
        if (lit && start < 0) {
            start = y;
        } else if (!lit && start >= 0) {
            runs.emplace_back(start, y - 1);
            start = -1;
        }
    }
    if (start >= 0) {
        runs.emplace_back(start, h - 1);
    }
    return runs;
}

bool InkColumnExtent(const uint8_t* mask, int w, int h, int y0, int y1, int& x0, int& x1) {
    if (!mask || w <= 0 || h <= 0) {
        return false;
    }
    y0 = std::max(0, y0);
    y1 = std::min(h, y1);
    int minX = w;
    int maxX = -1;
    for (int y = y0; y < y1; ++y) {
        const uint8_t* row = mask + (size_t)y * w;
        for (int x = 0; x < w; ++x) {
            if (row[x]) {
                if (x < minX) {
                    minX = x;
                }
                if (x > maxX) {
                    maxX = x;
                }
            }
        }
    }
    if (maxX < 0) {
        return false;
    }
    x0 = minX;
    x1 = maxX;
    return true;
}

void Colorize(const uint8_t* mask, int w, int h, const Layout& layout,
              Mode mode, const Palette& pal, double phase, bool cyclic,
              uint32_t seed, uint8_t* rgbOut) {
    if (!mask || !rgbOut || w <= 0 || h <= 0) {
        return;
    }
    const double ph = fract(phase);

    // A span mode whose geometry could not be measured (no ink, or a font that
    // would not report metrics) degrades to a horizontal gradient rather than
    // silently flattening to one colour -- the palette the user picked still
    // shows, which is a much better clue that something rendered than a block
    // of Colour 1 would be.
    bool spanMode = NeedsLetterSpans(mode) || NeedsWordSpans(mode) || NeedsLineSpans(mode);
    if (spanMode && layout.spans.empty()) {
        mode = Mode::GradientHorizontal;
        spanMode = false;
    }
    const bool randomSpans = (mode == Mode::RandomLetter || mode == Mode::RandomWord);

    std::vector<int32_t> spanIndex;
    if (spanMode) {
        spanIndex.assign((size_t)w * h, -1);
        for (const auto& s : layout.spans) {
            int sy0 = std::max(0, s.y0);
            int sy1 = std::min(h, s.y1);
            int sx0 = std::max(0, s.x0);
            int sx1 = std::min(w, s.x1);
            for (int y = sy0; y < sy1; ++y) {
                int32_t* row = spanIndex.data() + (size_t)y * w;
                for (int x = sx0; x < sx1; ++x) {
                    row[x] = s.index;
                }
            }
        }
    }

    const int lx = layout.x;
    const int ly = layout.y;
    // Normalised against the span BETWEEN the outermost lit pixels, not the
    // pixel count, so the last column of ink lands on 1.0 and a two-colour
    // gradient actually finishes on its second colour rather than stopping one
    // pixel short of it.
    const double lw = layout.w > 1 ? (double)(layout.w - 1) : 1.0;
    const double lh = layout.h > 1 ? (double)(layout.h - 1) : 1.0;
    const int spanCount = std::max(1, layout.spanCount);
    const uint32_t sparkleFrame = (uint32_t)(ph * (double)SPARKLE_STEPS);

    // Single is by far the most common case and has no per-pixel colour work at
    // all, so it gets its own loop rather than paying the switch below.
    if (mode == Mode::Single) {
        uint32_t c = pal.colors.empty() ? 0xFFFFFF : pal.colors[0];
        int cr = (c >> 16) & 0xFF, cg = (c >> 8) & 0xFF, cb = c & 0xFF;
        for (size_t i = 0, n = (size_t)w * h; i < n; ++i) {
            uint32_t cov = mask[i];
            uint8_t* o = rgbOut + i * 3;
            o[0] = (uint8_t)((cr * cov) / 255);
            o[1] = (uint8_t)((cg * cov) / 255);
            o[2] = (uint8_t)((cb * cov) / 255);
        }
        return;
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * w + x;
            uint32_t cov = mask[i];
            uint8_t* o = rgbOut + i * 3;
            if (!cov) {
                o[0] = o[1] = o[2] = 0;
                continue;
            }

            double tx = (double)(x - lx) / lw;
            double ty = (double)(y - ly) / lh;
            uint32_t c = 0xFFFFFF;
            double bright = 1.0;

            switch (mode) {
            case Mode::GradientHorizontal:
                c = rampColor(pal, tx + ph, cyclic);
                break;
            case Mode::GradientVertical:
                c = rampColor(pal, ty + ph, cyclic);
                break;
            case Mode::GradientDiagonal:
                c = rampColor(pal, (tx + ty) * 0.5 + ph, cyclic);
                break;
            case Mode::Sparkle: {
                c = rampColor(pal, tx + ph, cyclic);
                double u = hashUnit(seed, (uint32_t)i, sparkleFrame);
                if (u >= SPARKLE_WHITE_AT) {
                    c = 0xFFFFFF;
                } else {
                    bright = SPARKLE_FLOOR + (1.0 - SPARKLE_FLOOR) * (u / SPARKLE_WHITE_AT) * 0.6;
                }
                break;
            }
            case Mode::Chase: {
                c = rampColor(pal, tx, false);
                double d = fract(tx - ph);
                double dist = std::min(d, 1.0 - d);
                bright = CHASE_FLOOR + (1.0 - CHASE_FLOOR) *
                                           std::exp(-(dist * dist) / (2.0 * CHASE_SIGMA * CHASE_SIGMA));
                break;
            }
            default: {
                int32_t idx = spanIndex.empty() ? 0 : spanIndex[i];
                c = randomSpans ? randomSpanColor(pal, idx, ph, seed)
                                : spanColor(pal, idx, spanCount, ph);
                break;
            }
            }

            if (bright < 1.0) {
                c = scaleColor(c, bright);
            }
            o[0] = (uint8_t)((((c >> 16) & 0xFF) * cov) / 255);
            o[1] = (uint8_t)((((c >> 8) & 0xFF) * cov) / 255);
            o[2] = (uint8_t)(((c & 0xFF) * cov) / 255);
        }
    }
}

} // namespace TextColor

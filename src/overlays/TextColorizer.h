#pragma once
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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/*
 * Colour treatment for the Text pixel overlay effect.
 *
 * The effect renders its message ONCE into an 8-bit coverage mask (white
 * glyphs on black) and then paints that mask through one of the modes below.
 * Everything here is plain arithmetic over that mask with no GraphicsMagick
 * dependency, so an animated colour mode costs one pass over the mask per
 * frame rather than a full re-render of the text -- which matters because the
 * scrolling path already re-blits every frame on a Pi.
 *
 * Mode::Single reproduces the effect's original behaviour byte for byte (every
 * lit pixel gets the one fill colour), and is what an omitted/unknown mode
 * resolves to, so callers that never learned about any of this are unaffected.
 */
namespace TextColor {

enum class Mode {
    Single,
    GradientHorizontal,
    GradientVertical,
    GradientDiagonal,
    PerLetter,
    PerWord,
    PerLine,
    RandomLetter,
    RandomWord,
    Sparkle,
    Chase
};

/** The values the effect's ColorMode argument accepts, in UI order. */
const std::vector<std::string>& ModeNames();
/** Unrecognised names -- including "", so an omitted argument -- give Single. */
Mode ParseMode(const std::string& name);

/**
 * Which per-glyph geometry a mode needs measured. Modes that need none are a
 * pure function of the pixel position, so the caller can skip the (comparably
 * expensive) font metric passes entirely.
 */
bool NeedsLetterSpans(Mode m);
bool NeedsWordSpans(Mode m);
bool NeedsLineSpans(Mode m);
/** True when the painted result varies with `phase` -- i.e. worth repainting. */
bool IsAnimated(Mode m);

struct Palette {
    std::vector<uint32_t> colors; ///< 0x00RRGGBB, in order
    bool rainbow = false;         ///< ignore `colors`, sweep the hue circle
};

/** Values the effect's Palette argument accepts; "Custom" is first. */
const std::vector<std::string>& PaletteNames();
/**
 * Resolve a palette name. "Custom" (and anything unrecognised) yields the
 * caller's own colour list; an empty custom list falls back to white so a
 * mis-set palette still shows something rather than painting black on black.
 */
Palette BuildPalette(const std::string& name, const std::vector<uint32_t>& custom);

/** A run of glyphs -- one letter, word or line. x1/y1 are exclusive. */
struct Span {
    int index = 0; ///< ordinal within its group; picks the colour
    int x0 = 0;
    int x1 = 0;
    int y0 = 0;
    int y1 = 0;
};

struct Layout {
    // Ink bounding box. Gradients are normalised across THIS, not across the
    // whole image, so a short word centred on a wide panel still shows the
    // whole palette instead of a slice of it.
    int x = 0;
    int y = 0;
    int w = 1;
    int h = 1;
    std::vector<Span> spans; ///< empty unless the mode needs them
    int spanCount = 0;       ///< number of distinct indexes present in `spans`
};

/*
 * Ink analysis of a mask. All of it is derived from the lit pixels themselves
 * rather than from font metrics, so it stays correct however GraphicsMagick
 * ended up placing the text -- the effect only needs metrics for the one thing
 * pixels cannot tell it, which is where one letter stops and the next starts.
 */

/** Fill `out`'s bounding box. False (leaving it untouched) if nothing is lit. */
bool InkBounds(const uint8_t* mask, int w, int h, Layout& out);
/** Rows holding ink, grouped into runs of [first, last] separated by blank rows. */
std::vector<std::pair<int, int>> InkRowRuns(const uint8_t* mask, int w, int h);
/** Leftmost/rightmost lit column within rows [y0, y1). False if none are. */
bool InkColumnExtent(const uint8_t* mask, int w, int h, int y0, int y1, int& x0, int& x1);

/**
 * Paint `mask` (w*h coverage bytes) into `rgbOut` (w*h*3 bytes, fully
 * overwritten). `phase` is the animation position and is wrapped into [0,1);
 * pass 0 for a still frame. `cyclic` selects a wrapping palette ramp -- which
 * is seamless when animated -- over the plain first-colour-to-last ramp that a
 * still gradient wants.
 */
void Colorize(const uint8_t* mask, int w, int h, const Layout& layout,
              Mode mode, const Palette& pal, double phase, bool cyclic,
              uint32_t seed, uint8_t* rgbOut);

} // namespace TextColor

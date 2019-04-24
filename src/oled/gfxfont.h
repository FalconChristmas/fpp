#ifndef _GFXFONT_H_
#define _GFXFONT_H_

typedef struct {                                 // Data stored PER GLYPH
	unsigned short  bitmapOffset;                // Pointer into GFXfont->bitmap
	uint8_t        width, height;                // Bitmap dimensions in pixels
	uint8_t        xAdvance;                     // Distance to advance cursor (x axis)
	int8_t         xOffset, yOffset;             // Dist from cursor pos to UL corner
} GFXglyph, *GFXglyphPtr;

typedef struct {                                 // Data stored for FONT AS A WHOLE:
	uint8_t        *bitmap;                      // Glyph bitmaps, concatenated
	GFXglyphPtr     glyph;                       // Glyph array
	uint8_t         first, last;                 // ASCII extents
	uint8_t         yAdvance;                    // Newline distance (y axis)
    uint8_t         yMaxAscent;
} GFXfont, *GFXfontPtr;

#endif // _GFXFONT_H_

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

#include "fpp-pch.h"

#include "fpp-json.h"

#include <Magick++/Blob.h>
#include <Magick++/Color.h>
#include <Magick++/Geometry.h>
#include <Magick++/Image.h>
#include <Magick++/Include.h>
#include <Magick++/TypeMetric.h>
#include <magick/image.h>
#include <algorithm>
#include <cmath> // std::ceil/std::lround in the image scaling helpers
#include <cstdint>
#include <cstdlib>
#include <list>
#include <map>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

#include "../Warnings.h"
#include "../commands/Commands.h"
#include "../common.h"
#include "../log.h"
#include "../settings.h" // FPP_DIR_IMAGE for the Image effect

#include "PixelOverlay.h"
#include "PixelOverlayModel.h"
#include "WLEDEffects.h"

#include "PixelOverlayEffects.h"

// Map the long-form scroll directions used in the UI onto the short codes
// ImageMovementEffect understands.  Shared by the Text and Image effects.
static const std::string MapOverlayPosition(const std::string& p) {
    if (p == "Center") {
        return p;
    } else if (p == "Right to Left" || p == "R2L") {
        return "R2L";
    } else if (p == "Left to Right" || p == "L2R") {
        return "L2R";
    } else if (p == "Bottom to Top" || p == "B2T") {
        return "B2T";
    } else if (p == "Top to Bottom" || p == "T2B") {
        return "T2B";
    }
    return "Center";
}

void ScaleOverlayImage(Magick::Image& image, int w, int h, const std::string& mode) {
    if (w <= 0 || h <= 0 || image.columns() == 0 || image.rows() == 0) {
        return;
    }
    image.modifyImage();

    if (mode == "Stretch") {
        // aspect(true) is Magick's '!' flag: resize to exactly this geometry
        // rather than fitting inside it.
        Magick::Geometry g(w, h);
        g.aspect(true);
        image.resize(g);
        return;
    }

    if (mode == "Crop to Fill") {
        // Cover the box: the larger of the two scale factors is the one that
        // leaves no gap on either axis.  The overflow is then cropped centred.
        double sx = (double)w / (double)image.columns();
        double sy = (double)h / (double)image.rows();
        double s = std::max(sx, sy);
        Magick::Geometry g((size_t)std::ceil(image.columns() * s),
                           (size_t)std::ceil(image.rows() * s));
        g.aspect(true);
        image.resize(g);

        int xoff = ((int)image.columns() - w) / 2;
        int yoff = ((int)image.rows() - h) / 2;
        image.crop(Magick::Geometry(w, h, xoff < 0 ? 0 : xoff, yoff < 0 ? 0 : yoff));
        return;
    }

    if (mode != "Scale to Fit") {
        return; // "None" or unrecognized - leave the image alone
    }

    // Resize to slightly larger since trying to get exact can leave us off by
    // one pixel.  Going slightly larger lets us crop to exact size below.
    image.resize(Magick::Geometry(w + 4, h + 4, 0, 0));

    int cols = image.columns();
    int rows = image.rows();

    if (cols < w) // center horizontally
    {
        int diff = w - cols;

        image.borderColor(Magick::Color("black"));
        image.border(Magick::Geometry(diff / 2 + 1, 1, 0, 0));
    } else if (rows < h) // center vertically
    {
        int diff = h - rows;

        image.borderColor(Magick::Color("black"));
        image.border(Magick::Geometry(1, diff / 2 + 1, 0, 0));
    }

    image.crop(Magick::Geometry(w, h, 1, 1));
}

// Preserve aspect while matching the axis the image is NOT scrolling along, so
// a scrolling banner fills the model's height (or width) and runs off the ends.
static void scaleToCrossAxis(Magick::Image& image, int w, int h, bool horizontal) {
    if (image.columns() == 0 || image.rows() == 0) {
        return;
    }
    int nw, nh;
    if (horizontal) {
        nh = h;
        nw = (int)std::lround((double)image.columns() * h / (double)image.rows());
    } else {
        nw = w;
        nh = (int)std::lround((double)image.rows() * w / (double)image.columns());
    }
    nw = std::max(nw, 1);
    nh = std::max(nh, 1);

    image.modifyImage();
    Magick::Geometry g(nw, nh);
    g.aspect(true);
    image.resize(g);
}

// Extract an image into a freshly malloc'd RGB (3 bytes/pixel) buffer sized
// columns*rows*3.  The caller owns the buffer; ImageMovementEffect frees the
// one it is handed.  Returns nullptr on allocation/pixel-cache failure.
static uint8_t* imageToRGBBuffer(Magick::Image& image) {
    image.modifyImage();

    int cols = image.columns();
    int rows = image.rows();
    if (cols <= 0 || rows <= 0) {
        return nullptr;
    }

    const MagickLib::PixelPacket* pixel_cache = image.getConstPixels(0, 0, cols, rows);
    if (!pixel_cache) {
        return nullptr;
    }

    uint8_t* newData = (uint8_t*)malloc((size_t)cols * rows * 3);
    if (!newData) {
        return nullptr;
    }

    for (int yi = 0; yi < rows; yi++) {
        int idx = yi * cols;
        int nidx = yi * cols * 3;

        for (int xi = 0; xi < cols; xi++) {
            const MagickLib::PixelPacket* ptr2 = &pixel_cache[idx + xi];
            uint8_t* np = &newData[nidx + (xi * 3)];

            float r = Magick::Color::scaleQuantumToDouble(ptr2->red);
            float g = Magick::Color::scaleQuantumToDouble(ptr2->green);
            float b = Magick::Color::scaleQuantumToDouble(ptr2->blue);
            r *= 255;
            g *= 255;
            b *= 255;
            np[0] = r;
            np[1] = g;
            np[2] = b;
        }
    }
    return newData;
}

// Push a full model-sized RGB frame at the model, expanding to RGBW first if
// the model wants more than 3 bytes per pixel.
static void setModelDataFromRGB(PixelOverlayModel* m, const uint8_t* rgb) {
    int bpp = m->getBytesPerPixel();
    if (bpp == 3) {
        m->setData(rgb);
        return;
    }
    int pixels = m->getWidth() * m->getHeight();
    std::vector<uint8_t> expanded((size_t)pixels * bpp, 0);
    for (int p = 0; p < pixels; p++) {
        expanded[p * bpp] = rgb[p * 3];
        expanded[p * bpp + 1] = rgb[p * 3 + 1];
        expanded[p * bpp + 2] = rgb[p * 3 + 2];
    }
    m->setData(expanded.data());
}

bool DrawEncodedImageOnModel(PixelOverlayModel* m, const void* data, size_t len,
                             const std::string& scaling, int boxW, int boxH,
                             int xOffset, int yOffset, bool center,
                             const PixelOverlayState& st, bool toOverlayBuffer,
                             std::string& err) {
    if (!m || !data || len == 0) {
        err = "empty image body";
        return false;
    }

    Magick::Image image;
    try {
        image.quiet(true); // squelch warning exceptions
        Magick::Blob blob(data, len);
        image.read(blob);
        image.autoOrient();
        image.type(Magick::TrueColorType);
        image.depth(8);
        ScaleOverlayImage(image, boxW, boxH, scaling);
    } catch (Magick::Exception& error_) {
        err = std::string("could not decode image: ") + error_.what();
        return false;
    } catch (const std::exception& ex) {
        // Not everything thrown here is a Magick::Exception (std::bad_alloc,
        // for one), and an escape would unwind through the HTTP handler while
        // it holds the models lock.
        err = std::string("could not decode image: ") + ex.what();
        return false;
    }

    int cols = image.columns();
    int rows = image.rows();
    uint8_t* rgb = imageToRGBBuffer(image);
    if (!rgb) {
        err = "could not read pixels from the decoded image";
        return false;
    }

    int x = xOffset;
    int y = yOffset;
    if (center) {
        x += (boxW - cols) / 2;
        y += (boxH - rows) / 2;
    }

    bool ok = toOverlayBuffer ? m->blitOverlayBuffer(rgb, cols, rows, 3, x, y)
                              : m->blitData(rgb, cols, rows, 3, x, y, st);
    free(rgb);
    if (!ok) {
        err = "the image landed entirely outside the model";
    }
    return ok;
}

static uint32_t applyColorPct(uint32_t c, float pct) {
    uint32_t r = (c >> 16) & 0xFF;
    uint32_t g = (c >> 8) & 0xFF;
    uint32_t b = c & 0xFF;

    float t = r;
    t *= pct;
    r = (int)t;
    t = g;
    t *= pct;
    g = (int)t;
    t = b;
    t *= pct;
    b = (int)t;
    return (r << 16) + (g << 8) + b;
}

class StopRunningEffect : public RunningEffect {
public:
    StopRunningEffect(PixelOverlayModel* m, const std::string& n, bool ad) :
        RunningEffect(m),
        effectName(n),
        autoDisable(ad) {
    }
    StopRunningEffect(PixelOverlayModel* m, const std::string& n, const std::string& ad) :
        RunningEffect(m),
        effectName(n),
        autoDisable(false) {
        PixelOverlayState st(ad);
        if (st.getState() != PixelOverlayState::PixelState::Disabled) {
            autoDisable = true;
        }
    }
    const std::string& name() const override {
        return effectName;
    }
    virtual int32_t update() override {
        model->clearOverlayBuffer();
        model->flushOverlayBuffer();
        if (stopped) {
            if (autoDisable) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
            }
            return EFFECT_DONE;
        }
        stopped = true;
        return EFFECT_AFTER_NEXT_OUTPUT;
    }
    bool stopped = false;
    std::string effectName;
    bool autoDisable;
};

class ColorFadeEffect : public PixelOverlayEffect {
public:
    ColorFadeEffect() :
        PixelOverlayEffect("Color Fade") {
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("fadeIn", "int", "Fade In MS").setRange(0, 10000));
        args.push_back(CommandArg("fadeOut", "int", "Fade Out MS").setRange(0, 10000));
    }

    class CFRunningEffect : public RunningEffect {
    public:
        CFRunningEffect(PixelOverlayModel* m, const std::string& ae, const std::vector<std::string>& args) :
            RunningEffect(m),
            autoEnable(false) {
            color = PixelOverlayManager::mapColor(args[0]);
            fadeIn = std::stol(args[1]);
            fadeOut = std::stol(args[2]);

            PixelOverlayState st(ae);
            if (st.getState() != PixelOverlayState::PixelState::Disabled) {
                autoEnable = true;
                model->setState(st);
            }
            startTime = GetTimeMS();
            done = false;
            if (fadeIn == 0) {
                fill(color);
            } else {
                fill(0);
            }
        }
        const std::string& name() const override {
            static std::string NAME = "Color Fade";
            return NAME;
        }
        virtual void toJson(Json::Value& v) const override {
            v["name"] = name();
            v["color"] = color;
            v["fadeIn"] = fadeIn;
            v["fadeOut"] = fadeOut;
        };

        void fill(uint32_t c) {
            model->fill((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
        }
        virtual int32_t update() override {
            long long nowTime = GetTimeMS();
            nowTime -= startTime;

            if (nowTime < fadeIn) {
                // fading in
                float f = nowTime;
                f /= fadeIn;
                uint32_t c = applyColorPct(color, f);
                fill(c);
            } else if ((nowTime >= fadeIn) && (nowTime <= (fadeIn + fadeOut))) {
                // fading out
                nowTime -= fadeIn;
                float f = nowTime;
                f /= fadeOut;
                f = 1.0f - f;
                uint32_t c = applyColorPct(color, f);
                fill(c);
            } else if (!done && autoEnable) {
                // end
                if (!fadeOut) {
                    fill(color);
                } else {
                    fill(0x000000);
                }
                done = true;
                return EFFECT_AFTER_NEXT_OUTPUT;
            } else {
                if (autoEnable) {
                    model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
                } else if (!fadeOut) {
                    fill(color);
                } else {
                    fill(0x000000);
                }
                return 0;
            }
            return 25;
        }
        uint32_t color = 0xFFFFFF;
        long long startTime = 0;
        int fadeIn = 0;
        int fadeOut = 0;
        bool autoEnable;
        bool done;
    };

    virtual bool apply(PixelOverlayModel* model, const std::string& autoEnable, const std::vector<std::string>& args) override {
        if (args.size() != 3) {
            LogInfo(VB_CHANNELOUT, "ColorFadeEffect: not enough args\n");
            return false;
        }
        CFRunningEffect* re = new CFRunningEffect(model, autoEnable, args);
        model->setRunningEffect(re, 1);
        return true;
    }
};

class BarsEffect : public PixelOverlayEffect {
public:
    BarsEffect() :
        PixelOverlayEffect("Bars") {
        args.push_back(CommandArg("Direction", "string", "Direction").setContentList({ "Up", "Down", "Left", "Right" }));
        args.push_back(CommandArg("time", "int", "Time (ms)").setRange(1, 60000).setDefaultValue("2000"));
        args.push_back(CommandArg("iterations", "range", "Iterations").setRange(1, 30).setDefaultValue("1"));
        args.push_back(CommandArg("colorRep", "range", "Color Rep.").setRange(1, 5).setDefaultValue("1"));
        args.push_back(CommandArg("colors", "range", "Num Colors").setRange(1, 5).setDefaultValue("3"));
        args.push_back(CommandArg("Color1", "color", "Color 1").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Color2", "color", "Color 2").setDefaultValue("#00FF00"));
        args.push_back(CommandArg("Color3", "color", "Color 3").setDefaultValue("#0000FF"));
        args.push_back(CommandArg("Color4", "color", "Color 4").setDefaultValue("#FFFFFF"));
        args.push_back(CommandArg("Color5", "color", "Color 5").setDefaultValue("#000000"));
    }
    class BarsRunningEffect : public RunningEffect {
    public:
        BarsRunningEffect(PixelOverlayModel* m, const std::string& ae, const std::vector<std::string>& args) :
            RunningEffect(m),
            autoEnable(false) {
            if (args[0] == "Up") {
                direction = DirectionEnum::UP;
            } else if (args[0] == "Down") {
                direction = DirectionEnum::DOWN;
            } else if (args[0] == "Right") {
                direction = DirectionEnum::RIGHT;
            } else {
                direction = DirectionEnum::LEFT;
            }
            duration = std::stol(args[1]);
            iterations = std::stol(args[2]);
            int colorRep = std::stol(args[3]);
            if (colorRep < 1) {
                colorRep = 1;
            }
            int numColors = std::stol(args[4]);
            while (colorRep) {
                for (int x = 0; x < numColors; x++) {
                    colors.push_back(PixelOverlayManager::mapColor(args[5 + x]));
                }
                colorRep--;
            }
            PixelOverlayState st(ae);
            if (st.getState() != PixelOverlayState::PixelState::Disabled) {
                autoEnable = true;
                model->setState(st);
            }
            startTime = GetTimeMS();
            done = false;
            apply(0.0f);
        }
        const std::string& name() const override {
            static std::string NAME = "Bars";
            return NAME;
        }
        virtual void toJson(Json::Value& v) const override {
            v["name"] = name();
            switch (direction) {
            case DirectionEnum::UP:
                v["direction"] = "Up";
                break;
            case DirectionEnum::DOWN:
                v["direction"] = "Down";
                break;
            case DirectionEnum::LEFT:
                v["direction"] = "Left";
                break;
            case DirectionEnum::RIGHT:
                v["direction"] = "Right";
                break;
            }
            v["duration"] = duration;
            v["iterations"] = iterations;
            v["colors"] = Json::Value(Json::arrayValue);
            for (auto& c : colors) {
                v["colors"].append(c);
            }
        };

        void mapCoords(int m, int l, int w, int h, int& x, int& y) {
            switch (direction) {
            case DirectionEnum::UP:
                y = m;
                x = l;
                break;
            case DirectionEnum::DOWN:
                y = h - m - 1;
                x = l;
                break;
            case DirectionEnum::LEFT:
                y = l;
                x = m;
                break;
            case DirectionEnum::RIGHT:
                y = l;
                x = w - m - 1;
                break;
            default:
                x = y = 0;
            }
        }

        void apply(float pos) {
            int w, h;
            model->getSize(w, h);
            int max = w;
            int len = h;
            if (direction == DirectionEnum::UP || direction == DirectionEnum::DOWN) {
                max = h;
                len = w;
            }
            float numBars = colors.size();
            for (int m = 0; m < max; m++) {
                float f = m;
                f /= max;
                f += pos;
                if (f > 1.0) {
                    f -= 1.0;
                }
                f *= numBars;
                int cIdx = (int)f;
                if (cIdx >= numBars) {
                    cIdx = 0;
                }
                uint32_t color = colors[cIdx];
                int r = (color >> 16) & 0xFF;
                int g = (color >> 8) & 0xFF;
                int b = color & 0xFF;
                for (int l = 0; l < len; l++) {
                    int x, y;
                    mapCoords(m, l, w, h, x, y);
                    model->setPixelValue(x, y, r, g, b);
                }
            }
        }
        virtual int32_t update() override {
            long long nowTime = GetTimeMS();
            nowTime -= startTime;
            if (nowTime <= duration) {
                float f = nowTime;
                f /= duration;
                f *= iterations;
                while (f > 1.0f) {
                    f -= 1.0f;
                }
                apply(f);
            } else if (!done && autoEnable) {
                // end
                apply(1.0f);
                done = true;
                return EFFECT_AFTER_NEXT_OUTPUT;
            } else {
                if (autoEnable) {
                    model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
                } else {
                    apply(1.0f);
                }
                return 0;
            }
            return 25;
        }

        enum class DirectionEnum {
            UP,
            DOWN,
            LEFT,
            RIGHT
        } direction;
        long long startTime = 0;
        int iterations;
        int duration;
        std::vector<uint32_t> colors;
        bool autoEnable;
        bool done;
    };

    virtual bool apply(PixelOverlayModel* model, const std::string& autoEnable, const std::vector<std::string>& args) override {
        if (args.size() < 5) {
            LogInfo(VB_CHANNELOUT, "Bars Effect: not enough args\n");
            return false;
        }
        BarsRunningEffect* re = new BarsRunningEffect(model, autoEnable, args);
        model->setRunningEffect(re, 1);
        return true;
    }
};

class ImageMovementEffect : public RunningEffect {
public:
    ImageMovementEffect(PixelOverlayModel* m) :
        RunningEffect(m) {
    }

    virtual ~ImageMovementEffect() {
        if (imageData) {
            free(imageData);
        }
    }
    const std::string& name() const override {
        static std::string NAME = "Image";
        return NAME;
    }
    virtual void toJson(Json::Value& v) const override {
        v["name"] = name();
    }
    void copyImageData(int xoff, int yoff) {
        if (imageData) {
            model->clearOverlayBuffer();
            int h, w;
            model->getSize(w, h);
            int bpp = model->getBytesPerPixel();
            for (int y = 0; y < imageDataRows; ++y) {
                int ny = yoff + y;
                if (ny < 0 || ny >= h) {
                    continue;
                }

                // imageData is always RGB (3 bytes per pixel)
                uint8_t* src = imageData + (y * imageDataCols * 3);
                uint8_t* dst = model->getOverlayBuffer() + (ny * w * bpp);
                int pixelsToCopy = imageDataCols;

                if (xoff < 0) {
                    src -= xoff * 3;
                    pixelsToCopy += xoff;
                    if (pixelsToCopy >= w)
                        pixelsToCopy = w;
                } else if (xoff > 0) {
                    dst += xoff * bpp;
                    if (pixelsToCopy > (w - xoff))
                        pixelsToCopy = w - xoff;
                } else {
                    if (pixelsToCopy >= w)
                        pixelsToCopy = w;
                }

                if (pixelsToCopy > 0) {
                    if (bpp == 3) {
                        memcpy(dst, src, pixelsToCopy * 3);
                    } else {
                        // Expand RGB source to RGBW destination
                        for (int p = 0; p < pixelsToCopy; p++) {
                            dst[p * bpp] = src[p * 3];
                            dst[p * bpp + 1] = src[p * 3 + 1];
                            dst[p * bpp + 2] = src[p * 3 + 2];
                            if (bpp > 3) {
                                dst[p * bpp + 3] = 0;
                            }
                        }
                    }
                }
            }
            model->flushOverlayBuffer();
        }
    }
    virtual int32_t update() override {
        if (!done) {
            if (endTimeMS && GetTimeMS() >= endTimeMS) {
                // Duration expired mid-scroll.  Wipe the partial frame so a
                // timeout leaves the model blank, matching what the static path
                // does, rather than freezing half an image on the display.
                //
                // Return a timed delay rather than EFFECT_AFTER_NEXT_OUTPUT:
                // the completion handshake below itself waits on an output
                // cycle, and returning AFTER_NEXT_OUTPUT here consumes the one
                // our flush just produced, leaving the effect wedged holding
                // the model enabled forever.  This mirrors what the natural
                // scroll-off end does - flush a frame, return a delay.
                done = true;
                model->clearOverlayBuffer();
                model->flushOverlayBuffer();
                return 1000 / std::max(1, speed);
            }
            if (direction == "R2L") {
                --x;
                if (x <= (-imageDataCols)) {
                    done = true;
                }
            } else if (direction == "L2R") {
                ++x;
                if (x >= model->getWidth()) {
                    done = true;
                }
            } else if (direction == "B2T") {
                --y;
                if (y <= (-imageDataRows)) {
                    done = true;
                }
            } else if (direction == "T2B") {
                ++y;
                if (y >= model->getHeight()) {
                    done = true;
                }
            }
            copyImageData(x, y);
            // A UI/command speed of 0 is in range for both the Text and Image
            // effects; dividing by it raised SIGFPE and took fppd down.
            int msDelay = 1000 / std::max(1, speed);
            return msDelay;
        }
        if (disableWhenDone) {
            if (waitingToSetState) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
            } else {
                waitingToSetState = true;
                return EFFECT_AFTER_NEXT_OUTPUT;
            }
        }
        return EFFECT_DONE;
    }

    bool done = false;
    bool waitingToSetState = false;
    uint8_t* imageData = nullptr;
    int imageDataRows = 0;
    int imageDataCols = 0;

    std::string direction;
    int x = 0;
    int y = 0;
    int speed = 0;
    bool disableWhenDone = false;

    // Absolute stop time in ms, or 0 to run until the image scrolls off the
    // model.  Only the Image effect sets this; the Text effect leaves it 0 so
    // its long-standing "scroll once, ignore Duration" behaviour is unchanged.
    long long endTimeMS = 0;
};

class TextEffect : public PixelOverlayEffect {
public:
    TextEffect() :
        PixelOverlayEffect("Text") {
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Font", "string", "Font").setContentListUrl("api/overlays/fonts", false));
        args.push_back(CommandArg("FontSize", "range", "FontSize").setRange(4, 100).setDefaultValue("18"));
        args.push_back(CommandArg("FontAntiAlias", "bool", "Anti-Aliased").setDefaultValue("false"));
        args.push_back(CommandArg("Position", "string", "Position").setContentList({ "Center", "Right to Left", "Left to Right", "Bottom to Top", "Top to Bottom" }));
        args.push_back(CommandArg("Speed", "range", "Scroll Speed").setRange(0, 200).setDefaultValue("10"));
        args.push_back(CommandArg("Duration", "int", "Duration").setRange(-1, 2000).setDefaultValue("0"));

        // keep text as last argument if possible as the MQTT commands will, by default, use the payload of the mqtt
        // msg as the last argument.  Thus, this allows all of the above to be topic paths, but the text to be
        // sent in the payload
        args.push_back(CommandArg("Text", "string", "Text").setAdjustable());
    }
    class TextMovementEffect : public ImageMovementEffect {
    public:
        TextMovementEffect(PixelOverlayModel* m) :
            ImageMovementEffect(m) {
        }

        const std::string& name() const override {
            static std::string NAME = "Text";
            return NAME;
        }
    };

    void doText(PixelOverlayModel* m,
                const std::string& msg,
                int r, int g, int b,
                const std::string& font,
                int fontSize,
                bool antialias,
                const std::string& position,
                int pixelsPerSecond,
                const std::string& autoEnable,
                int duration) {
        bool disableWhenDone = false;
        PixelOverlayState st(autoEnable);
        if ((st.getState() != PixelOverlayState::PixelState::Disabled) && (m->getState().getState() == PixelOverlayState::PixelState::Disabled)) {
            m->setState(st);
            disableWhenDone = true;
        }

        // Normalize a literal "\n" (backslash followed by 'n', as typed into a
        // single-line text field) into a real newline byte so it is treated the
        // same as an actual embedded newline for both sizing and rendering below.
        // A doubled backslash ("\\") escapes to a single literal backslash, so
        // text containing a literal "\n" sequence (e.g. a Windows path like
        // "c:\network") can be entered as "c:\\network" without being split.
        std::string normalizedMsg;
        normalizedMsg.reserve(msg.length());
        for (int x = 0; x < msg.length(); x++) {
            if (msg[x] == '\\' && (x < msg.length() - 1) && msg[x + 1] == 'n') {
                normalizedMsg += '\n';
                x++;
            } else if (msg[x] == '\\' && (x < msg.length() - 1) && msg[x + 1] == '\\') {
                normalizedMsg += '\\';
                x++;
            } else {
                normalizedMsg += msg[x];
            }
        }

        Magick::Image* image = new Magick::Image(Magick::Geometry(m->getWidth(), m->getHeight()), Magick::Color("black"));
        image->quiet(true);
        image->depth(8);
        image->font(font);
        image->fontPointsize(fontSize);
        image->antiAlias(antialias);

        int maxWid = 0;
        int totalHi = 0;

        int last = 0;
        for (int x = 0; x < normalizedMsg.length(); x++) {
            if (normalizedMsg[x] == '\n') {
                std::string newM = normalizedMsg.substr(last, x - last);
                Magick::TypeMetric metrics;
                image->fontTypeMetrics(newM, &metrics);
                maxWid = std::max(maxWid, (int)metrics.textWidth());
                totalHi += (int)metrics.textHeight();
                last = x + 1;
            }
        }
        std::string newM = normalizedMsg.substr(last);
        Magick::TypeMetric metrics;
        image->fontTypeMetrics(newM, &metrics);
        maxWid = std::max(maxWid, (int)metrics.textWidth());
        totalHi += (int)metrics.textHeight();

        // Empty text (or text consisting only of newlines) measures as zero width,
        // which would make the scrolling branch below construct an invalid image.
        maxWid = std::max(maxWid, 1);
        totalHi = std::max(totalHi, 1);

        if (position == "Centered" || position == "Center") {
            image->magick("RGB");
            // one shot, just draw the text and return
            double rr = r;
            double rg = g;
            double rb = b;
            rr /= 255.0f;
            rg /= 255.0f;
            rb /= 255.0f;

            image->fillColor(Magick::Color(Magick::Color::scaleDoubleToQuantum(rr),
                                           Magick::Color::scaleDoubleToQuantum(rg),
                                           Magick::Color::scaleDoubleToQuantum(rb)));
            image->antiAlias(antialias);
            image->strokeAntiAlias(antialias);
            image->annotate(normalizedMsg, Magick::CenterGravity);
            Magick::Blob blob;
            image->write(&blob);

            setModelDataFromRGB(m, (const uint8_t*)blob.data());

            if (disableWhenDone) {
                int nd = 25;
                if (duration > 0) {
                    nd = duration * 1000;
                }
                m->setRunningEffect(new StopRunningEffect(m, "Text", disableWhenDone), nd);
            }
            delete image;
        } else {
            delete image;
            // movement
            double rr = r;
            double rg = g;
            double rb = b;
            rr /= 255.0f;
            rg /= 255.0f;
            rb /= 255.0f;

            Magick::Image image2(Magick::Geometry(maxWid, totalHi), Magick::Color("black"));
            image2.quiet(true);
            image2.depth(8);
            image2.font(font);
            image2.fontPointsize(fontSize);
            image2.antiAlias(antialias);

            image2.fillColor(Magick::Color(Magick::Color::scaleDoubleToQuantum(rr),
                                           Magick::Color::scaleDoubleToQuantum(rg),
                                           Magick::Color::scaleDoubleToQuantum(rb)));
            image2.antiAlias(antialias);
            image2.strokeAntiAlias(antialias);
            image2.annotate(normalizedMsg, Magick::CenterGravity);

            double y = (m->getHeight() / 2.0) - ((totalHi) / 2.0);
            double x = (m->getWidth() / 2.0) - (maxWid / 2.0);
            if (position == "R2L") {
                x = m->getWidth();
            } else if (position == "L2R") {
                x = -maxWid;
            } else if (position == "B2T") {
                y = m->getHeight();
            } else if (position == "T2B") {
                y = -metrics.ascent();
            }
            uint8_t* newData = imageToRGBBuffer(image2);
            if (!newData) {
                LogErr(VB_CHANNELOUT, "Text Overlay Effect - could not read rendered text pixels\n");
                return;
            }

            std::unique_lock<std::recursive_mutex> lock(m->getRunningEffectMutex());
            TextMovementEffect* ef = dynamic_cast<TextMovementEffect*>(m->getRunningEffect());
            if (ef == nullptr) {
                ef = new TextMovementEffect(m);
                ef->x = (int)x;
                ef->y = (int)y;
            }
            ef->speed = pixelsPerSecond;
            ef->disableWhenDone = disableWhenDone;
            ef->direction = position;
            int32_t t = 1000 / pixelsPerSecond;
            if (t == 0) {
                t = 1;
            }
            uint8_t* old = ef->imageData;
            ef->imageData = newData;
            ef->imageDataCols = image2.columns();
            ef->imageDataRows = image2.rows();
            ef->copyImageData(ef->x, ef->y);
            m->setRunningEffect(ef, t);
            lock.unlock();
            free(old);
        }
    }

    virtual bool apply(PixelOverlayModel* model, const std::string& autoEnable, const std::vector<std::string>& args) override {
        std::string color = args[0];
        unsigned int cint = PixelOverlayManager::mapColor(color);
        std::string font = PixelOverlayManager::INSTANCE.mapFont(args[1]);
        if (font.empty()) {
            std::string warn = "Text Overlay Effect - Could not find font \'" + args[1] + "\'";
            WarningHolder::AddWarning(warn);
            return false;
        }
        int fontSize = std::atoi(args[2].c_str());
        if (fontSize < 4) {
            fontSize = 12;
        }
        bool aa = args[3] == "true" || args[3] == "1";
        std::string position = MapOverlayPosition(args[4]);
        int pps = std::atoi(args[5].c_str());
        int duration = std::atoi(args[6].c_str());
        std::string msg = args[7];

        doText(model,
               msg,
               (cint >> 16) & 0xFF,
               (cint >> 8) & 0xFF,
               cint & 0xFF,
               font,
               fontSize,
               aa,
               position,
               pps,
               autoEnable,
               duration);
        return true;
    }
};

class ImageEffect : public PixelOverlayEffect {
public:
    ImageEffect() :
        PixelOverlayEffect("Image") {
        args.push_back(CommandArg("Scaling", "string", "Scaling").setContentList({ "Scale to Fit", "Crop to Fill", "Stretch", "None" }).setDefaultValue("Scale to Fit"));
        args.push_back(CommandArg("Position", "string", "Position").setContentList({ "Center", "Right to Left", "Left to Right", "Bottom to Top", "Top to Bottom" }));
        args.push_back(CommandArg("Speed", "range", "Scroll Speed").setRange(0, 200).setDefaultValue("10"));
        args.push_back(CommandArg("Duration", "int", "Duration (0 = leave up)").setRange(0, 2000).setDefaultValue("0"));

        // keep the filename as the last argument so the MQTT commands can, by
        // default, carry it as the payload while everything above is a topic
        // path.  Same reasoning as the Text effect's Text argument.
        args.push_back(CommandArg("Image", "string", "Image").setContentListUrl("api/overlays/images", false).setAdjustable());
    }

    // Compose an arbitrarily sized RGB image into a model-sized RGB frame,
    // centred, clipping whatever falls outside.  Used for the static case so a
    // "None"-scaled or odd-sized image can never run off the end of setData's
    // width*height*bpp buffer.
    void centerIntoFrame(const uint8_t* src, int srcCols, int srcRows,
                         std::vector<uint8_t>& frame, int w, int h) {
        int xoff = (w - srcCols) / 2;
        int yoff = (h - srcRows) / 2;

        for (int y = 0; y < srcRows; y++) {
            int ny = yoff + y;
            if (ny < 0 || ny >= h) {
                continue;
            }
            int sx = 0;
            int dx = xoff;
            int count = srcCols;
            if (dx < 0) {
                sx = -dx;
                count += dx;
                dx = 0;
            }
            if (count > (w - dx)) {
                count = w - dx;
            }
            if (count <= 0) {
                continue;
            }
            memcpy(&frame[((size_t)ny * w + dx) * 3],
                   src + (((size_t)y * srcCols + sx) * 3),
                   (size_t)count * 3);
        }
    }

    bool doImage(PixelOverlayModel* m,
                 const std::string& path,
                 const std::string& scaling,
                 const std::string& position,
                 int pixelsPerSecond,
                 const std::string& autoEnable,
                 int duration) {
        if (path.empty()) {
            WarningHolder::AddWarningTimeout(60, 34, "Image Overlay Effect - no image specified");
            return false;
        }

        std::string fullPath = startsWith(path, "/") ? path : FPP_DIR_IMAGE("/" + path);
        if (!FileExists(fullPath)) {
            WarningHolder::AddWarningTimeout(60, 34, "Image Overlay Effect - file not found: " + fullPath);
            LogErr(VB_CHANNELOUT, "Image Overlay Effect - file not found: %s\n", fullPath.c_str());
            return false;
        }

        int w = m->getWidth();
        int h = m->getHeight();
        bool horizontal = (position == "R2L") || (position == "L2R");

        // Decode and scale before touching the model or the effect slot: a slow
        // read here should not leave a half-applied effect behind.
        Magick::Image image;
        try {
            image.quiet(true); // squelch warning exceptions
            image.read(fullPath);
            image.autoOrient();
            image.type(Magick::TrueColorType);
            image.depth(8);

            if (position == "Center" || scaling != "Scale to Fit") {
                ScaleOverlayImage(image, w, h, scaling);
            } else {
                // Scrolling with aspect preserved: match the axis we are not
                // travelling along and let the other run past the model edge.
                scaleToCrossAxis(image, w, h, horizontal);
            }
        } catch (Magick::Exception& error_) {
            LogErr(VB_CHANNELOUT, "GraphicsMagick exception reading %s: %s\n", fullPath.c_str(), error_.what());
            WarningHolder::AddWarningTimeout(60, 34, "Image Overlay Effect - could not read " + fullPath);
            return false;
        } catch (const std::exception& ex) {
            // Not everything thrown here is a Magick::Exception (std::bad_alloc,
            // filesystem errors), and an escape would unwind through the
            // command dispatch holding the models lock.
            LogErr(VB_CHANNELOUT, "Exception preparing image %s: %s\n", fullPath.c_str(), ex.what());
            WarningHolder::AddWarningTimeout(60, 34, "Image Overlay Effect - could not read " + fullPath);
            return false;
        }

        bool disableWhenDone = false;
        PixelOverlayState st(autoEnable);
        if ((st.getState() != PixelOverlayState::PixelState::Disabled) && (m->getState().getState() == PixelOverlayState::PixelState::Disabled)) {
            m->setState(st);
            disableWhenDone = true;
        }

        uint8_t* imageRGB = imageToRGBBuffer(image);
        if (!imageRGB) {
            LogErr(VB_CHANNELOUT, "Image Overlay Effect - could not read pixels from %s\n", fullPath.c_str());
            return false;
        }

        int cols = image.columns();
        int rows = image.rows();

        if (position == "Center") {
            std::vector<uint8_t> frame((size_t)w * h * 3, 0);
            centerIntoFrame(imageRGB, cols, rows, frame, w, h);
            free(imageRGB);

            // We are taking the model over, so drop any effect still running on
            // it: its next tick would clear or overwrite the image we are about
            // to draw.  The Text effect gets this incidentally because it always
            // installs a StopRunningEffect here; with Duration 0 meaning "leave
            // it up" we install nothing, so the eviction has to be explicit.
            // The scrolling path below needs no equivalent - setRunningEffect
            // deletes the previous effect for us.
            {
                std::unique_lock<std::recursive_mutex> lock(m->getRunningEffectMutex());
                if (m->getRunningEffect()) {
                    m->setRunningEffect(nullptr, 25); // one no-op tick, then dropped
                }
            }

            setModelDataFromRGB(m, frame.data());

            // Duration > 0 clears the image (and disables the model if we were
            // the one that enabled it) after that many seconds; 0 leaves it up
            // until something else changes the model.  This deliberately
            // differs from the Text effect, where Duration is honoured only in
            // the auto-enable case and a Duration of 0 wipes the model after
            // 25ms - which for an image means the default arguments would draw
            // and immediately erase.  Text's behaviour is left alone so
            // existing presets do not shift under people.
            if (duration > 0) {
                m->setRunningEffect(new StopRunningEffect(m, "Image", disableWhenDone), duration * 1000);
            }
            return true;
        }

        // Scrolling: hand the pixels to the same mover the Text effect uses.
        double y = (h / 2.0) - (rows / 2.0);
        double x = (w / 2.0) - (cols / 2.0);
        if (position == "R2L") {
            x = w;
        } else if (position == "L2R") {
            x = -cols;
        } else if (position == "B2T") {
            y = h;
        } else if (position == "T2B") {
            y = -rows;
        }

        std::unique_lock<std::recursive_mutex> lock(m->getRunningEffectMutex());
        ImageMovementEffect* ef = new ImageMovementEffect(m);
        ef->x = (int)x;
        ef->y = (int)y;
        ef->speed = std::max(1, pixelsPerSecond);
        ef->disableWhenDone = disableWhenDone;
        ef->direction = position;
        if (duration > 0) {
            ef->endTimeMS = GetTimeMS() + ((long long)duration * 1000);
        }
        ef->imageData = imageRGB; // effect owns it from here, frees in its dtor
        ef->imageDataCols = cols;
        ef->imageDataRows = rows;
        ef->copyImageData(ef->x, ef->y);

        int32_t t = 1000 / ef->speed;
        if (t == 0) {
            t = 1;
        }
        m->setRunningEffect(ef, t);
        return true;
    }

    virtual bool apply(PixelOverlayModel* model, const std::string& autoEnable, const std::vector<std::string>& args) override {
        if (args.size() < 5) {
            LogInfo(VB_CHANNELOUT, "Image Effect: not enough args\n");
            return false;
        }
        std::string scaling = args[0];
        std::string position = MapOverlayPosition(args[1]);
        int pps = std::atoi(args[2].c_str());
        int duration = std::atoi(args[3].c_str());
        std::string file = args[4];

        return doImage(model, file, scaling, position, pps, autoEnable, duration);
    }
};

class StopEffect : public PixelOverlayEffect {
public:
    StopEffect() :
        PixelOverlayEffect("Stop Effects") {
    }
    virtual bool apply(PixelOverlayModel* model, const std::string& autoEnable, const std::vector<std::string>& args) override {
        model->setRunningEffect(new StopRunningEffect(model, "Stop Effects", autoEnable), 25);
        return true;
    }
};

class PixelOverlayEffectHolder {
public:
    PixelOverlayEffectHolder() {
        add(new ColorFadeEffect());
        add(new BarsEffect());
        add(new TextEffect());
        add(new ImageEffect());

        std::list<PixelOverlayEffect*> wled = WLEDEffect::getWLEDEffects();
        for (auto a : wled) {
            add(a);
        }
        effectNames.sort();
        StopEffect* pe = new StopEffect();
        effectNames.push_front(pe->name);
        effects[pe->name] = pe;
    }
    ~PixelOverlayEffectHolder() {
        for (auto& a : effects) {
            delete a.second;
        }
        effects.clear();
        WLEDEffect::cleanupWLEDEffects();
    }
    void add(PixelOverlayEffect* pe) {
        effects[pe->name] = pe;
        effectNames.push_back(pe->name);
    }
    void remove(PixelOverlayEffect* pe) {
        effects.erase(pe->name);
        auto it = std::find(effectNames.begin(), effectNames.end(), pe->name);
        if (it != effectNames.end()) {
            effectNames.erase(it);
        }
    }
    PixelOverlayEffect* get(const std::string& n) {
        return effects[n];
    }
    const std::list<std::string>& names() const {
        return effectNames;
    }

private:
    std::map<std::string, PixelOverlayEffect*> effects;
    std::list<std::string> effectNames;
};

static PixelOverlayEffectHolder EFFECTS;

PixelOverlayEffect* PixelOverlayEffect::GetPixelOverlayEffect(const std::string& name) {
    return EFFECTS.get(name);
}

const std::list<std::string>& PixelOverlayEffect::GetPixelOverlayEffects() {
    return EFFECTS.names();
}

void PixelOverlayEffect::AddPixelOverlayEffect(PixelOverlayEffect* effect) {
    EFFECTS.add(effect);
}
void PixelOverlayEffect::RemovePixelOverlayEffect(PixelOverlayEffect* effect) {
    EFFECTS.remove(effect);
}

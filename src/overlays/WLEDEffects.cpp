/*
*   Pixel Overlay Effects for Falcon Player (FPP) ported from WLED
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

#include "PixelOverlay.h"
#include "PixelOverlayModel.h"
#include "WLEDEffects.h"

#include "wled/FX.h"

WLEDEffect::WLEDEffect(const std::string& name) :
    PixelOverlayEffect(name) {
}
WLEDEffect::~WLEDEffect() {
}

class WLEDRunningEffect : public RunningEffect {
public:
    WLEDRunningEffect(PixelOverlayModel* m, const std::string& n, const std::string& ad) :
        RunningEffect(m),
        effectName(n),
        autoDisable(false) {
        PixelOverlayState st(ad);
        if (st.getState() != PixelOverlayState::PixelState::Disabled) {
            autoDisable = true;
            model->setState(st);
        }
    }
    const std::string& name() const override {
        return effectName;
    }

    virtual int32_t doIteration() = 0;

    virtual int32_t update() override {
        if (stopped) {
            if (autoDisable) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
            }
            return EFFECT_DONE;
        }
        int r = doIteration();
        if (r <= 0) {
            stopped = true;
            return EFFECT_AFTER_NEXT_OUTPUT;
        }
        return r;
    }
    bool stopped = false;
    std::string effectName;
    bool autoDisable;

    static int parseInt(const std::string& setting) {
        return std::atoi(setting.c_str());
    }
    static uint32_t parseColor(const std::string& setting) {
        return PixelOverlayManager::mapColor(setting);
    }
    static uint32_t adjustColor(uint32_t color, int brightness) {
        int r = color & 0xFF0000;
        r >>= 16;
        int g = color & 0xFF00;
        g >>= 8;
        int b = color & 0xFF;

        r *= brightness;
        r /= 100;
        r &= 0xFF;
        g *= brightness;
        g /= 100;
        g &= 0xFF;
        b *= brightness;
        b /= 100;
        b &= 0xFF;
        return (r << 16 | g << 8 | b);
    }

    void fill(uint32_t color) {
        model->fillOverlayBuffer((color & 0xFF0000) >> 16, (color & 0xFF00) >> 8, color & 0xFF);
    }
    void flush() {
        model->flushOverlayBuffer();
    }
};

class BlinkEffect : public WLEDEffect {
public:
    BlinkEffect() :
        WLEDEffect("Blink") {
        args.push_back(CommandArg("Brightness", "range", "Brightness").setRange(0, 100).setDefaultValue("100"));
        args.push_back(CommandArg("Speed", "range", "Speed").setRange(1, 5000).setDefaultValue("1000"));
        args.push_back(CommandArg("Intensity", "range", "Intensity").setRange(0, 100).setDefaultValue("50"));
        args.push_back(CommandArg("Color1", "color", "Color1").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Color2", "color", "Color2").setDefaultValue("#000000"));
    }

    virtual bool apply(PixelOverlayModel* model, const std::string& autoEnable, const std::vector<std::string>& args) override {
        model->setRunningEffect(new BlinkEffectInternal(model, "Blink", autoEnable, args), 25);
        return true;
    }

    class BlinkEffectInternal : public WLEDRunningEffect {
    public:
        BlinkEffectInternal(PixelOverlayModel* m, const std::string& n, const std::string& ad, const std::vector<std::string>& args) :
            WLEDRunningEffect(m, n, ad) {
            brightness = parseInt(args[0]);
            speed = parseInt(args[1]);
            intensity = parseInt(args[2]);
            color1 = adjustColor(parseColor(args[3]), brightness);
            color2 = adjustColor(parseColor(args[4]), brightness);
        }
        virtual int32_t doIteration() {
            if (isColor2) {
                fill(color1);
                isColor2 = false;
                flush();
                return speed * intensity / 100;
            }
            fill(color2);
            flush();
            isColor2 = true;
            return speed - (speed * intensity / 100);
        }

        int brightness;
        int speed;
        int intensity;
        uint32_t color1;
        uint32_t color2;
        bool isColor2 = true;
    };
};

static std::vector<std::string> BUFFERMAPS;
static std::vector<std::string> PALETTES;
extern float GetChannelOutputRefreshRate();

class RawWLEDEffect : public WLEDEffect {
public:
    RawWLEDEffect(const std::string& name, int m) :
        WLEDEffect(name),
        mode(m) {
        fillVectors();
        args.push_back(CommandArg("BufferMapping", "string", "Buffer Mapping").setContentList(BUFFERMAPS));
        args.push_back(CommandArg("Brightness", "range", "Brightness").setRange(0, 255).setDefaultValue("128"));
        args.push_back(CommandArg("Speed", "range", "Speed").setRange(0, 255).setDefaultValue("128"));
        args.push_back(CommandArg("Intensity", "range", "Intensity").setRange(0, 255).setDefaultValue("128"));
        args.push_back(CommandArg("Palette", "string", "Palette").setContentList(PALETTES).setDefaultValue("Default"));
        args.push_back(CommandArg("Color1", "color", "Color1").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Color2", "color", "Color2").setDefaultValue("#0000FF"));
        args.push_back(CommandArg("Color3", "color", "Color3").setDefaultValue("#000000"));
    }
    static void fillVectors() {
        if (PALETTES.empty()) {
            const char** p = wled_palette_names;
            while (*p) {
                std::string n = *p;
                PALETTES.push_back(n);
                p++;
            }
        }
        if (BUFFERMAPS.empty()) {
            BUFFERMAPS.push_back("Horizontal");
            BUFFERMAPS.push_back("Vertical");
            BUFFERMAPS.push_back("Horizontal Flipped");
            BUFFERMAPS.push_back("Vertical Flipped");
        }
    }

    virtual bool apply(PixelOverlayModel* model, const std::string& autoEnable, const std::vector<std::string>& args) override {
        model->setRunningEffect(new RawWLEDEffectInternal(model, name, autoEnable, args, mode), 25);
        return true;
    }

    class RawWLEDEffectInternal : public WLEDRunningEffect {
    public:
        RawWLEDEffectInternal(PixelOverlayModel* m, const std::string& n, const std::string& ad, const std::vector<std::string>& args, int mode) :
            WLEDRunningEffect(m, n, ad) {
            RawWLEDEffect::fillVectors();

            int mapping = std::find(BUFFERMAPS.begin(), BUFFERMAPS.end(), args[0]) - BUFFERMAPS.begin();

            brightness = parseInt(args[1]);
            speed = parseInt(args[2]);
            intensity = parseInt(args[3]);
            palette = args[4];
            color1 = parseColor(args[5]);
            color2 = parseColor(args[6]);
            color3 = parseColor(args[7]);

            int p = 0;
            RawWLEDEffect::fillVectors();
            for (int x = 0; x < PALETTES.size(); x++) {
                if (PALETTES[x] == palette) {
                    p = x;
                }
            }
            wled = new WS2812FX(m, mapping);
            wled->setMode(0, mode);
            wled->setColor(0, color1);
            wled->setColor(1, color2);
            wled->setColor(2, color3);
            wled->setEffectConfig(mode, speed, intensity, p);
            wled->milliampsPerLed = 0; //need to turn off the power calculation
            wled->setBrightness(brightness);
        }
        virtual ~RawWLEDEffectInternal() {
            delete wled;
        }
        virtual int32_t doIteration() {
            wled->service();

            // no sense updating faster than the output
            float f = 1000.0f / GetChannelOutputRefreshRate();
            int i = f;
            return i;
        }

        int brightness;
        int speed;
        int intensity;
        uint32_t color1;
        uint32_t color2;
        uint32_t color3;
        std::string palette;

        WS2812FX* wled;
    };

    int mode;
};

std::list<PixelOverlayEffect*> WLEDEffect::getWLEDEffects() {
    std::list<PixelOverlayEffect*> v;
    v.push_back(new BlinkEffect());

    for (int x = 2; x < MODE_COUNT; x++) {
        std::string name = "WLED - ";
        name += wled_mode_names[x];
        v.push_back(new RawWLEDEffect(name, x));
    }
    return v;
}

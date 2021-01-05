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

#include "WLEDEffects.h"
#include "PixelOverlay.h"
#include "PixelOverlayModel.h"


WLEDEffect::WLEDEffect(const std::string &name) : PixelOverlayEffect(name) {
}
WLEDEffect::~WLEDEffect() {
}

class WLEDRunningEffect : public RunningEffect {
public:
    WLEDRunningEffect(PixelOverlayModel *m, const std::string &n, const std::string &ad)
        : RunningEffect(m), effectName(n), autoDisable(false) {
        PixelOverlayState st(ad);
        if (st.getState() != PixelOverlayState::PixelState::Disabled) {
            autoDisable = true;
            model->setState(st);
        }
    }
    const std::string &name() const override {
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
    
    
    static int parseInt(const std::string &setting) {
        return std::atoi(setting.c_str());
    }
    static uint32_t parseColor(const std::string &setting) {
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
        model->fillOverlayBuffer((color & 0xFF0000) >> 16 , (color & 0xFF00) >> 8, color & 0xFF);
    }
    void flush() {
        model->flushOverlayBuffer();
    }
};


class BlinkEffect : public WLEDEffect {
public:
    BlinkEffect() : WLEDEffect("Blink") {
        args.push_back(CommandArg("Brightness", "range", "Brightness").setRange(0, 100).setDefaultValue("100"));
        args.push_back(CommandArg("Speed", "range", "Speed").setRange(1, 5000).setDefaultValue("1000"));
        args.push_back(CommandArg("Intensity", "range", "Intensity").setRange(0, 100).setDefaultValue("50"));
        args.push_back(CommandArg("Color1", "color", "Color1").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Color2", "color", "Color2").setDefaultValue("#000000"));
    }
    
    
    virtual bool apply(PixelOverlayModel *model, const std::string &autoEnable, const std::vector<std::string> &args) override {
        model->setRunningEffect(new BlinkEffectInternal(model, "Blink", autoEnable, args), 25);
        return true;
    }
    
    class BlinkEffectInternal : public WLEDRunningEffect {
    public:
        BlinkEffectInternal(PixelOverlayModel *m, const std::string &n, const std::string &ad, const std::vector<std::string> &args)
        : WLEDRunningEffect(m, n, ad) {
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







std::list<PixelOverlayEffect *> WLEDEffect::getWLEDEffects() {
    std::list<PixelOverlayEffect *> v;
    v.push_back(new BlinkEffect());
    
    return v;
}


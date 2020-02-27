/*
*   Pixel Overlay Effects for Falcon Player (FPP)
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


#include "PixelOverlayEffects.h"
#include "PixelOverlayModel.h"
#include "PixelOverlay.h"
#include "log.h"
#include "common.h"
#include <map>

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

class ColorFadeEffect : public PixelOverlayEffect {
public:
    ColorFadeEffect() : PixelOverlayEffect("Color Fade") {
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("fadeIn", "int", "Fade In MS").setRange(0, 10000));
        args.push_back(CommandArg("fadeOut", "int", "Fade Out MS").setRange(0, 10000));
    }
    
    class CFRunningEffect : public RunningEffect {
    public:
        CFRunningEffect(PixelOverlayModel *m, bool ae, const std::vector<std::string> &args) : RunningEffect(m), autoEnable(ae) {
            color = PixelOverlayManager::mapColor(args[0]);
            fadeIn = std::stol(args[1]);
            fadeOut = std::stol(args[2]);
            if (autoEnable) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
            }
            startTime = GetTimeMS();
            done = false;
            if (fadeIn == 0) {
                fill(color);
            } else {
                fill(0);
            }
        }
        void fill(uint32_t c) {
            model->fill((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
        }
        virtual int32_t update() override {
            long long nowTime = GetTimeMS();
            nowTime -= startTime;

            if (nowTime < fadeIn) {
                //fading in
                float f = nowTime;
                f /= fadeIn;
                uint32_t c = applyColorPct(color, f);
                fill(c);
            } else if ((nowTime >= fadeIn) && (nowTime <= (fadeIn+fadeOut))) {
                //fading out
                nowTime -= fadeIn;
                float f = nowTime;
                f /= fadeOut;
                f = 1.0f - f;
                uint32_t c = applyColorPct(color, f);
                fill(c);
            } else if (!done && autoEnable) {
                //end
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
    
    virtual bool apply(PixelOverlayModel *model, bool autoEnable, const std::vector<std::string> &args) override {
        if (args.size() != 3) {
            LogInfo(VB_CHANNELOUT, "ColorFadeEffect: not enough args\n");
            return false;
        }
        CFRunningEffect *re = new CFRunningEffect(model, autoEnable, args);
        model->setRunningEffect(re, 1);
        return true;
    }
};



class BarsEffect : public PixelOverlayEffect {
public:
    BarsEffect() : PixelOverlayEffect("Bars") {
        args.push_back(CommandArg("Direction", "string", "Direction").setContentList({"Up", "Down", "Left", "Right"}));
        args.push_back(CommandArg("time", "int", "Time (ms)").setRange(1, 60000).setDefaultValue("2000"));
        args.push_back(CommandArg("iterations", "int", "Iterations").setRange(1, 30).setDefaultValue("1"));
        args.push_back(CommandArg("colorRep", "int", "Color Rep.").setRange(1, 5).setDefaultValue("1"));
        args.push_back(CommandArg("colors", "int", "Num Colors").setRange(1, 5).setDefaultValue("3"));
        args.push_back(CommandArg("Color1", "color", "Color 1").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Color2", "color", "Color 2").setDefaultValue("#00FF00"));
        args.push_back(CommandArg("Color3", "color", "Color 3").setDefaultValue("#0000FF"));
        args.push_back(CommandArg("Color4", "color", "Color 4").setDefaultValue("#FFFFFF"));
        args.push_back(CommandArg("Color5", "color", "Color 5").setDefaultValue("#000000"));
    }
    class BarsRunningEffect : public RunningEffect {
    public:
        BarsRunningEffect(PixelOverlayModel *m, bool ae, const std::vector<std::string> &args) : RunningEffect(m), autoEnable(ae) {
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
            if (autoEnable) {
                model->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
            }
            startTime = GetTimeMS();
            done = false;
            apply(0.0f);
        }
        void mapCoords(int m, int l, int w, int h, int &x, int &y) {
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
                f /=duration;
                f *= iterations;
                while (f > 1.0f) {
                    f -= 1.0f;
                }
                apply(f);
            } else if (!done && autoEnable) {
                //end
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
            UP, DOWN, LEFT, RIGHT
        } direction;
        long long startTime = 0;
        int iterations;
        int duration;
        std::vector<uint32_t> colors;
        bool autoEnable;
        bool done;
    };
    
    virtual bool apply(PixelOverlayModel *model, bool autoEnable, const std::vector<std::string> &args) override {
        if (args.size() < 5) {
            LogInfo(VB_CHANNELOUT, "Bars Effect: not enough args\n");
            return false;
        }
        BarsRunningEffect *re = new BarsRunningEffect(model, autoEnable, args);
        model->setRunningEffect(re, 1);
        return true;
    }
};


class PixelOverlayEffectHolder {
public:
    PixelOverlayEffectHolder() {
        add(new ColorFadeEffect());
        add(new BarsEffect());
    }
    ~PixelOverlayEffectHolder() {
        for (auto &a : effects) {
            delete a.second;
        }
        effects.clear();
    }
    void add(PixelOverlayEffect *pe) {
        effects[pe->name] = pe;
        effectNames.push_back(pe->name);
    }
    PixelOverlayEffect *get(const std::string &n) {
        return effects[n];
    }
    const std::vector<std::string> &names() const {
        return effectNames;
    }

private:
    std::map<std::string, PixelOverlayEffect*> effects;
    std::vector<std::string> effectNames;
};
static PixelOverlayEffectHolder EFFECTS;

PixelOverlayEffect* PixelOverlayEffect::GetPixelOverlayEffect(const std::string &name) {
    return EFFECTS.get(name);
}

const std::vector<std::string> &PixelOverlayEffect::GetPixelOverlayEffects() {
    return EFFECTS.names();
}

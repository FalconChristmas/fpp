/*
 * Pixel Overlay Effects for Falcon Player (FPP) ported from WLED
 *
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
    static int parseBool(const std::string& setting) {
        return setting == "true" || setting == "1";
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

static std::vector<std::string>* BUFFERMAPS = nullptr;
static std::vector<std::string>* PALETTES = nullptr;
extern float GetChannelOutputRefreshRate();

enum class ArgMapping {
    Mapping,
    Brightness,
    Speed,
    Intensity,
    Palette,
    Color1,
    Color2,
    Color3,
    Custom1,
    Custom2,
    Custom3,
    Check1,
    Check2,
    Check3,
    Text
};

const char* defaultSliderName(int idx, ArgMapping& m) {
    switch (idx) {
    case 0:
        m = ArgMapping::Speed;
        return "Speed";
    case 1:
        m = ArgMapping::Intensity;
        return "Intensity";
    case 2:
        m = ArgMapping::Custom1;
        return "Custom 1";
    case 3:
        m = ArgMapping::Custom2;
        return "Custom 2";
    case 4:
        m = ArgMapping::Custom3;
        return "Custom 3";
    case 5:
        m = ArgMapping::Check1;
        return "Check 1";
    case 6:
        m = ArgMapping::Check2;
        return "Check 2";
    case 7:
        m = ArgMapping::Check3;
        return "Check 3";
    }
    m = ArgMapping::Custom1;
    return "Custom";
}
const char* defaultColorName(int idx, ArgMapping& m, std::string& defVal) {
    if (idx == 0) {
        m = ArgMapping::Color1;
        defVal = "#FF0000";
        return "Color 1";
    }
    if (idx == 1) {
        m = ArgMapping::Color2;
        defVal = "#0000FF";
        return "Color 2";
    }
    m = ArgMapping::Color3;
    defVal = "#000000";
    return "Color 3";
}

class RawWLEDEffect : public WLEDEffect {
public:
    RawWLEDEffect(const std::string& name, const std::string& config, int m) :
        WLEDEffect(name),
        mode(m) {
        fillVectors();
        args.push_back(CommandArg("BufferMapping", "string", "Buffer Mapping").setContentList(*BUFFERMAPS));
        argMap.push_back(ArgMapping::Mapping); // always have mapping
        args.push_back(CommandArg("Brightness", "range", "Brightness").setRange(0, 255).setDefaultValue("128"));
        argMap.push_back(ArgMapping::Brightness); // always have brightness

        std::string c = config;
        size_t t = c.find("@");
        if (t != std::string::npos) {
            c = c.substr(t + 1);
            std::string sliders = c;
            t = c.find(";");
            if (t != std::string::npos) {
                c = c.substr(t + 1);
                sliders = sliders.substr(0, t);
            }
            std::string pallete = c;
            t = c.find(";");
            if (t != std::string::npos) {
                c = c.substr(t + 1);
                pallete = pallete.substr(0, t);
            }
            int idx = 0;
            // printf("%s        -%s-\n", config.c_str(), c.c_str());
            while (!sliders.empty()) {
                ArgMapping m;
                std::string sname = defaultSliderName(idx, m);
                if (sliders[0] == '!') {
                    sliders = sliders.substr(1);
                    if (!sliders.empty() && sliders[0] == ',') {
                        sliders = sliders.substr(1);
                    }
                } else {
                    size_t comma = sliders.find(',');
                    if (comma != std::string::npos) {
                        sname = sliders.substr(0, comma);
                        sliders = sliders.substr(comma + 1);
                    } else {
                        sname = sliders;
                        sliders = "";
                    }
                }
                // printf("    s%d: %s\n", idx, sname.c_str());
                if (sname != "") {
                    if (m == ArgMapping::Custom3) {
                        // custom3 is
                        args.push_back(CommandArg(sname, "range", sname).setRange(0, 31).setDefaultValue("16"));
                    } else if (m >= ArgMapping::Check1 && m <= ArgMapping::Check3) {
                        args.push_back(CommandArg(sname, "bool", sname).setDefaultValue("0"));
                    } else {
                        // speed, intensity, custom1, custom2 are full range
                        args.push_back(CommandArg(sname, "range", sname).setRange(0, 255).setDefaultValue("128"));
                    }
                    argMap.push_back(m);
                }
                idx++;
            }
            args.push_back(CommandArg("Palette", "string", "Palette").setContentList(*PALETTES).setDefaultValue("Default"));
            argMap.push_back(ArgMapping::Palette); // Palette
            if (!pallete.empty()) {
                idx = 0;
                while (!pallete.empty()) {
                    ArgMapping m;
                    std::string val;
                    std::string cname = defaultColorName(idx, m, val);
                    if (pallete[0] == '!') {
                        pallete = pallete.substr(1);
                        if (!pallete.empty() && pallete[0] == ',') {
                            pallete = pallete.substr(1);
                        }
                    } else {
                        size_t comma = pallete.find(',');
                        if (comma != std::string::npos) {
                            cname = pallete.substr(0, comma);
                            pallete = pallete.substr(comma + 1);
                        } else {
                            cname = pallete;
                            pallete = "";
                        }
                    }
                    // printf("    p%d: %s    %s\n", idx, cname.c_str(), val.c_str());
                    if (cname != "") {
                        args.push_back(CommandArg(cname, "color", cname).setDefaultValue(val));
                        argMap.push_back(m);
                    }
                    idx++;
                }
            } else {
                args.push_back(CommandArg("Color 1", "color", "Color 1").setDefaultValue("#FF0000"));
                args.push_back(CommandArg("Color 2", "color", "Color 2").setDefaultValue("#0000FF"));
                args.push_back(CommandArg("Color 3", "color", "Color 3").setDefaultValue("#000000"));
                argMap.push_back(ArgMapping::Color1); // c1
                argMap.push_back(ArgMapping::Color2); // c2
                argMap.push_back(ArgMapping::Color3); // c3
            }
        } else {
            args.push_back(CommandArg("Speed", "range", "Speed").setRange(0, 255).setDefaultValue("128"));
            args.push_back(CommandArg("Intensity", "range", "Intensity").setRange(0, 255).setDefaultValue("128"));
            argMap.push_back(ArgMapping::Speed);     // speed
            argMap.push_back(ArgMapping::Intensity); // intensity

            args.push_back(CommandArg("Palette", "string", "Palette").setContentList(*PALETTES).setDefaultValue("Default"));
            args.push_back(CommandArg("Color1", "color", "Color1").setDefaultValue("#FF0000"));
            args.push_back(CommandArg("Color2", "color", "Color2").setDefaultValue("#0000FF"));
            args.push_back(CommandArg("Color3", "color", "Color3").setDefaultValue("#000000"));
            argMap.push_back(ArgMapping::Palette); // Palette
            argMap.push_back(ArgMapping::Color1);  // c1
            argMap.push_back(ArgMapping::Color2);  // c2
            argMap.push_back(ArgMapping::Color3);  // c3
        }
        if (name.find("Text") != std::string::npos) {
            args.push_back(CommandArg("Text", "string", "Text"));
            argMap.push_back(ArgMapping::Text);
        }
    }
    static void fillVectors() {
        if (PALETTES == nullptr) {
            PALETTES = new std::vector<std::string>();
            const char* p = JSON_palette_names;
            while (*p != '\"') {
                ++p;
            }
            ++p;
            while (*p) {
                const char* p2 = p;
                while (*p2 != '\"') {
                    ++p2;
                }
                std::string n = p;
                n = n.substr(0, p2 - p);
                PALETTES->push_back(n);
                p = p2;
                ++p;
                while (*p && *p != '\"') {
                    ++p;
                }
                ++p;
            }
        }
        if (BUFFERMAPS == nullptr) {
            BUFFERMAPS = new std::vector<std::string>();
            BUFFERMAPS->emplace_back("Horizontal");
            BUFFERMAPS->emplace_back("Vertical");
            BUFFERMAPS->emplace_back("Horizontal Flipped");
            BUFFERMAPS->emplace_back("Vertical Flipped");
        }
    }

    virtual bool apply(PixelOverlayModel* model, const std::string& autoEnable, const std::vector<std::string>& args) override {
        model->setRunningEffect(new RawWLEDEffectInternal(model, name, autoEnable, args, mode, argMap), 25);
        return true;
    }

    class RawWLEDEffectInternal : public WLEDRunningEffect {
    public:
        RawWLEDEffectInternal(PixelOverlayModel* m, const std::string& n, const std::string& ad, const std::vector<std::string>& args, int mode, const std::vector<ArgMapping> argMap) :
            WLEDRunningEffect(m, n, ad) {
            RawWLEDEffect::fillVectors();

            int mapping = 0;
            bool hasC3 = false;
            int count = args.size();
            if (count > argMap.size()) {
                count = argMap.size();
            }
            for (int x = 0; x < count; x++) {
                switch (argMap[x]) {
                case ArgMapping::Mapping:
                    mapping = std::find(BUFFERMAPS->begin(), BUFFERMAPS->end(), args[x]) - BUFFERMAPS->begin();
                    break;
                case ArgMapping::Brightness:
                    brightness = parseInt(args[x]);
                    break;
                case ArgMapping::Speed:
                    speed = parseInt(args[x]);
                    break;
                case ArgMapping::Intensity:
                    intensity = parseInt(args[x]);
                    break;
                case ArgMapping::Custom1:
                    custom1 = parseInt(args[x]);
                    break;
                case ArgMapping::Custom2:
                    custom2 = parseInt(args[x]);
                    break;
                case ArgMapping::Custom3:
                    custom3 = parseInt(args[x]);
                    break;
                case ArgMapping::Check1:
                    check1 = parseBool(args[x]);
                    break;
                case ArgMapping::Check2:
                    check2 = parseBool(args[x]);
                    break;
                case ArgMapping::Check3:
                    check3 = parseBool(args[x]);
                    break;
                case ArgMapping::Palette:
                    palette = args[x];
                    break;
                case ArgMapping::Color1:
                    color1 = parseColor(args[x]);
                    break;
                case ArgMapping::Color2:
                    color2 = parseColor(args[x]);
                    break;
                case ArgMapping::Color3:
                    hasC3 = true;
                    color3 = parseColor(args[x]);
                    break;
                case ArgMapping::Text:
                    text = args[x];
                    break;
                default:
                    printf("Don't know what to do with %d\n", argMap[x]);
                    break;
                }
            }

            int p = 0;
            RawWLEDEffect::fillVectors();
            for (int x = 0; x < PALETTES->size(); x++) {
                if ((*PALETTES)[x] == palette) {
                    p = x;
                }
            }
            if (!hasC3) {
                color3 = color1;
            }
            wled = new WS2812FXExt(m, mapping, brightness,
                                   mode, speed, intensity, p,
                                   color1, color2, color3,
                                   custom1, custom2, custom3,
                                   check1, check2, check3, text);
            WS2812FXExt::popCurrent();
        }
        virtual ~RawWLEDEffectInternal() {
            delete wled;
        }
        virtual int32_t doIteration() {
            WS2812FXExt::pushCurrent(wled);
            wled->service();
            WS2812FXExt::popCurrent();

            wled->model->flushOverlayBuffer();
            // no sense updating faster than the output
            float f = 1000.0f / GetChannelOutputRefreshRate();
            int i = f;
            return i;
        }

        int brightness = 128;
        int speed = 128;
        int intensity = 128;
        uint32_t color1 = 0xFF0000;
        uint32_t color2 = 0x0000FF;
        uint32_t color3 = 0x000000;
        std::string palette = "Default";
        int custom1 = 128;
        int custom2 = 128;
        int custom3 = 128;
        int check1 = 0;
        int check2 = 0;
        int check3 = 0;
        std::string text;

        WS2812FXExt* wled;
    };

    int mode;
    std::vector<ArgMapping> argMap;
};

std::list<PixelOverlayEffect*> WLEDEffect::getWLEDEffects() {
    std::list<PixelOverlayEffect*> v;
    v.push_back(new BlinkEffect());

    WS2812FX* inst = WS2812FX::getInstance();
    if (inst == nullptr) {
        inst = new WS2812FXExt();
    }
    int i = inst->getModeCount();
    for (int x = 2; x < i; x++) {
        std::string name = "WLED - ";
        if (x > 127) {
            // audio reactive effects, add a note so people know these are special
            name = "WLED♪ - ";
        }
        std::string ename = inst->getModeData(x);
        if (ename != "RSVD") {
            while (ename.find('+') != std::string::npos) {
                int idx = ename.find('+');
                ename = ename.substr(0, idx) + "-Plus" + ename.substr(idx + 1);
            }
            if (ename.find('@') != std::string::npos) {
                name += ename.substr(0, ename.find('@'));
            } else {
                name += ename;
            }
            v.push_back(new RawWLEDEffect(name, ename, x));
        }
    }
    return v;
}
void WLEDEffect::cleanupWLEDEffects() {
    delete BUFFERMAPS;
    BUFFERMAPS = nullptr;
    delete PALETTES;
    PALETTES = nullptr;
    WS2812FXExt* inst = (WS2812FXExt*)WS2812FX::getInstance();
    WS2812FX::clearInstance();
    if (inst != nullptr) {
        delete inst;
    }
}
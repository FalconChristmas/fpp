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

#include <Magick++.h>
#include <magick/type.h>

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

class StopRunningEffect : public RunningEffect {
public:
    StopRunningEffect(PixelOverlayModel *m, const std::string &n, bool ad)
        : RunningEffect(m), effectName(n), autoDisable(ad) {
    }
    const std::string &name() const override {
        return effectName;
    }
    virtual int32_t update() {
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
        const std::string &name() const override {
            static std::string NAME = "Color Fade";
            return NAME;
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
        const std::string &name() const override {
            static std::string NAME = "Bars";
            return NAME;
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

class ImageMovementEffect : public RunningEffect {
public:
    ImageMovementEffect(PixelOverlayModel *m) : RunningEffect(m) {
    }
    
    virtual ~ImageMovementEffect() {
        if (imageData) {
            free(imageData);
        }
    }
    void copyImageData(int xoff, int yoff) {
        if (imageData) {
            model->clearOverlayBuffer();
            int h, w;
            model->getSize(w, h);
            for (int y = 0; y < imageDataRows; ++y)  {
                int ny = yoff + y;
                if (ny < 0 || ny >= h) {
                    continue;
                }

                uint8_t *src = imageData + (y * imageDataCols * 3);
                uint8_t *dst = model->getOverlayBuffer() + (ny * w * 3);
                int pixelsToCopy = imageDataCols;
                int c = w * 3;

                if (xoff < 0) {
                    src -= xoff * 3;
                    pixelsToCopy += xoff;
                    if (pixelsToCopy >= w)
                        pixelsToCopy = w;
                } else if (xoff > 0) {
                    dst += xoff * 3;
                    if (pixelsToCopy > (w - xoff))
                        pixelsToCopy = w - xoff;
                } else {
                    if (pixelsToCopy >= w)
                        pixelsToCopy = w;
                }

                if (pixelsToCopy > 0)
                    memcpy(dst, src, pixelsToCopy * 3);
            }
            model->flushOverlayBuffer();
        }
    }
    virtual int32_t update() {
        if (!done) {
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
            int msDelay = 1000 / speed;
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
    uint8_t *imageData = nullptr;
    int imageDataRows = 0;
    int imageDataCols = 0;
    
    std::string direction;
    int x = 0;
    int y = 0;
    int speed = 0;
    bool disableWhenDone = false;
};


class TextEffect : public PixelOverlayEffect {
public:
    TextEffect() : PixelOverlayEffect("Text") {
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Font", "string", "Font").setContentListUrl("api/overlays/fonts", false));
        args.push_back(CommandArg("FontSize", "int", "FontSize").setRange(4, 100).setDefaultValue("18"));
        args.push_back(CommandArg("FontAntiAlias", "bool", "Anti-Aliased").setDefaultValue("false"));
        args.push_back(CommandArg("Position", "string", "Position").setContentList({"Center", "Right to Left", "Left to Right", "Bottom to Top", "Top to Bottom"}));
        args.push_back(CommandArg("Speed", "int", "Scroll Speed").setRange(0, 200).setDefaultValue("10"));
        args.push_back(CommandArg("Duration", "int", "Duration").setRange(-1, 2000).setDefaultValue("0"));

        // keep text as last argument if possible as the MQTT commands will, by default, use the payload of the mqtt
        // msg as the last argument.  Thus, this allows all of the above to be topic paths, but the text to be
        // sent in the payload
        args.push_back(CommandArg("Text", "string", "Text").setAdjustable());
    }
    class TextMovementEffect : public ImageMovementEffect {
    public:
        TextMovementEffect(PixelOverlayModel *m) : ImageMovementEffect(m) {
        }
        
        const std::string &name() const override {
            static std::string NAME = "Text";
            return NAME;
        }

        
    };
    
    const std::string mapPosition(const std::string &p) {
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

    void doText(PixelOverlayModel *m,
                const std::string &msg,
                int r, int g, int b,
                const std::string &font,
                int fontSize,
                bool antialias,
                const std::string &position,
                int pixelsPerSecond,
                bool autoEnable,
                int duration) {

        Magick::Image image(Magick::Geometry(m->getWidth(),m->getHeight()), Magick::Color("black"));
        image.quiet(true);
        image.depth(8);
        image.font(font);
        image.fontPointsize(fontSize);
        image.antiAlias(antialias);

        bool disableWhenDone = false;
        if ((autoEnable) && (m->getState().getState() == PixelOverlayState::PixelState::Disabled))
        {
            m->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
            disableWhenDone = true;
        }
        
        int maxWid = 0;
        int totalHi = 0;
        
        int lines = 1;
        int last = 0;
        for (int x = 0; x < msg.length(); x++) {
            if (msg[x] == '\n' || ((x < msg.length() - 1) && msg[x] == '\\' && msg[x+1] == 'n')) {
                lines++;
                std::string newM = msg.substr(last, x);
                Magick::TypeMetric metrics;
                image.fontTypeMetrics(newM, &metrics);
                maxWid = std::max(maxWid,  (int)metrics.textWidth());
                totalHi += (int)metrics.textHeight();
                if (msg[x] == '\n') {
                    last = x + 1;
                } else {
                    last = x + 2;
                }
            }
        }
        std::string newM = msg.substr(last);
        Magick::TypeMetric metrics;
        image.fontTypeMetrics(newM, &metrics);
        maxWid = std::max(maxWid,  (int)metrics.textWidth());
        totalHi += (int)metrics.textHeight();
        
        if (position == "Centered" || position == "Center") {
            image.magick("RGB");
            //one shot, just draw the text and return
            double rr = r;
            double rg = g;
            double rb = b;
            rr /= 255.0f;
            rg /= 255.0f;
            rb /= 255.0f;
            
            image.fillColor(Magick::Color(Magick::Color::scaleDoubleToQuantum(rr),
                                          Magick::Color::scaleDoubleToQuantum(rg),
                                          Magick::Color::scaleDoubleToQuantum(rb)));
            image.antiAlias(antialias);
            image.strokeAntiAlias(antialias);
            image.annotate(msg, Magick::CenterGravity);
            Magick::Blob blob;
            image.write( &blob );
            
            m->setData((uint8_t*)blob.data());

            if (disableWhenDone) {
                int nd = 25;
                if (duration > 0) {
                    nd = duration * 1000;
                }
                m->setRunningEffect(new StopRunningEffect(m, "Text", disableWhenDone), duration);
            }
        } else {
            //movement
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
            image2.annotate(msg, Magick::CenterGravity);

            double y = (m->getHeight() / 2.0) - ((totalHi) / 2.0);
            double x = (m->getWidth() / 2.0) - (maxWid / 2.0);
            if (position == "R2L") {
                x = m->getWidth();
            } else if (position == "L2R") {
                x = -maxWid;
            } else if (position == "B2T") {
                y = m->getHeight();
            } else if (position == "T2B") {
                y =  -metrics.ascent();
            }
            image2.modifyImage();
            
            
            TextMovementEffect *ef = dynamic_cast<TextMovementEffect*>(m->getRunningEffect());
            if (ef == nullptr) {
                ef = new TextMovementEffect(m);
                ef->x = (int)x;
                ef->y = (int)y;
            }

            const MagickLib::PixelPacket *pixel_cache = image2.getConstPixels(0,0, image2.columns(), image2.rows());
            uint8_t *newData = (uint8_t*)malloc(image2.columns() * image2.rows() * 3);
            for (int yi = 0; yi < image2.rows(); yi++) {
                int idx = yi * image2.columns();
                int nidx = yi * image2.columns() * 3;

                for (int xi = 0; xi < image2.columns(); xi++) {
                    const MagickLib::PixelPacket *ptr2 = &pixel_cache[idx + xi];
                    uint8_t *np = &newData[nidx + (xi*3)];
                    
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
            ef->speed = pixelsPerSecond;
            ef->disableWhenDone = disableWhenDone;
            ef->direction = position;
            int32_t t = 1000/pixelsPerSecond;
            if (t == 0) {
                t = 1;
            }
            uint8_t *old= ef->imageData;
            ef->imageData = newData;
            ef->imageDataCols = image2.columns();
            ef->imageDataRows = image2.rows();
            ef->copyImageData(ef->x, ef->y);
            m->setRunningEffect(ef, t);
            free(old);
        }
    }

    
    virtual bool apply(PixelOverlayModel *model, bool autoEnable, const std::vector<std::string> &args) override {
        std::string color = args[0];
        unsigned int cint = PixelOverlayManager::mapColor(color);
        std::string font = PixelOverlayManager::INSTANCE.mapFont(args[1]);
        int fontSize = std::atoi(args[2].c_str());
        if (fontSize < 4) {
            fontSize = 12;
        }
        bool aa = args[3] == "true" || args[3] == "1";
        std::string position = mapPosition(args[4]);
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


class StopEffect : public PixelOverlayEffect {
public:
    StopEffect() : PixelOverlayEffect("Stop Effects") {
    }
    virtual bool apply(PixelOverlayModel *model, bool autoEnable, const std::vector<std::string> &args) override {
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
        add(new StopEffect());
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

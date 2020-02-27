/*
*   Pixel Overlay Models for Falcon Player (FPP)
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


#include <stdio.h>
#include <cstring>

#include <jsoncpp/json/json.h>
#include <Magick++.h>
#include <magick/type.h>

#include "PixelOverlayModel.h"
#include "PixelOverlay.h"
#include "PixelOverlayEffects.h"

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
            memset(model->getOverlayBuffer(), 0, model->getBlock()->channelCount);
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
            model->setData(model->getOverlayBuffer());
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
        model->unlock();
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

PixelOverlayModel::PixelOverlayModel(FPPChannelMemoryMapControlBlock *b,
                                     const std::string &n,
                                     char         *cdm,
                                     uint32_t     *pm)
    : block(b), name(n), chanDataMap(cdm), pixelMap(pm),
    overlayBuffer(nullptr), runningEffect(nullptr)
{
}
PixelOverlayModel::~PixelOverlayModel() {
    if (overlayBuffer) {
        free(overlayBuffer);
    }
}

int PixelOverlayModel::getWidth() const {
    int h, w;
    getSize(w, h);
    return w;
}
int PixelOverlayModel::getHeight() const {
    int h, w;
    getSize(w, h);
    return h;
}
void PixelOverlayModel::getSize(int &w, int &h) const {
    h = block->stringCount * block->strandsPerString;
    w = block->channelCount / 3;
    w /= h;

    if (block->orientation == 'V') {
        std::swap(w, h);
    }
}
PixelOverlayState PixelOverlayModel::getState() const {
    int i = block->isActive;
    return PixelOverlayState(i);
}
void PixelOverlayModel::setState(const PixelOverlayState &state) {
    int i = (int)state.getState();
    block->isActive = i;
}

void PixelOverlayModel::setData(const uint8_t *data) {
    for (int c = 0; c < block->channelCount; c++) {
        chanDataMap[pixelMap[block->startChannel + c - 1]] = data[c];
    }
}
void PixelOverlayModel::fill(int r, int g, int b) {
    int start = block->startChannel - 1;
    int end = block->startChannel + block->channelCount - 2;
    
    for (int c = start; c <= end;) {
        chanDataMap[pixelMap[c++]] = r;
        chanDataMap[pixelMap[c++]] = g;
        chanDataMap[pixelMap[c++]] = b;
    }
}
void PixelOverlayModel::setValue(uint8_t value, int startChannel, int endChannel) {
    int start;
    int end;
    
    if (startChannel != -1) {
        start = startChannel >= block->startChannel ? startChannel : block->startChannel;
    } else {
        start = block->startChannel;
    }
    
    int modelEnd = block->startChannel + block->channelCount - 1;
    if (endChannel != -1) {
        end = endChannel <= modelEnd ? endChannel : modelEnd;
    } else {
        end = modelEnd;
    }
    
    // Offset for zero-based arrays
    start--;
    end--;
    
    for (int c = start; c <= end; c++) {
        chanDataMap[pixelMap[c]] = value;
    }
}
void PixelOverlayModel::setPixelValue(int x, int y, int r, int g, int b) {
    int c = block->startChannel - 1 + (y*getWidth()*3) + x*3;
    chanDataMap[pixelMap[c++]] = r;
    chanDataMap[pixelMap[c++]] = g;
    chanDataMap[pixelMap[c++]] = b;
}

void PixelOverlayModel::doText(const std::string &msg,
                               int r, int g, int b,
                               const std::string &font,
                               int fontSize,
                               bool antialias,
                               const std::string &position,
                               int pixelsPerSecond,
                               bool autoEnable) {

    Magick::Image image(Magick::Geometry(getWidth(),getHeight()), Magick::Color("black"));
    image.quiet(true);
    image.depth(8);
    image.font(font);
    image.fontPointsize(fontSize);
    image.antiAlias(antialias);

    bool disableWhenDone = false;
    if ((autoEnable) && (getState().getState() == PixelOverlayState::PixelState::Disabled))
    {
        setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
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
        
        setData((uint8_t*)blob.data());

        if (disableWhenDone)
            setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
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

        double y = (getHeight() / 2.0) - ((totalHi) / 2.0);
        double x = (getWidth() / 2.0) - (maxWid / 2.0);
        if (position == "R2L") {
            x = getWidth();
        } else if (position == "L2R") {
            x = -maxWid;
        } else if (position == "B2T") {
            y = getHeight();
        } else if (position == "T2B") {
            y =  -metrics.ascent();
        }
        image2.modifyImage();
        
        ImageMovementEffect *ef = new ImageMovementEffect(this);
        
        const MagickLib::PixelPacket *pixel_cache = image2.getConstPixels(0,0, image2.columns(), image2.rows());
        ef->imageData = (uint8_t*)malloc(image2.columns() * image2.rows() * 3);
        ef->imageDataCols = image2.columns();
        ef->imageDataRows = image2.rows();
        for (int yi = 0; yi < ef->imageDataRows; yi++) {
            int idx = yi * ef->imageDataCols;
            int nidx = yi * ef->imageDataCols * 3;

            for (int xi = 0; xi < image2.columns(); xi++) {
                const MagickLib::PixelPacket *ptr2 = &pixel_cache[idx + xi];
                uint8_t *np = &ef->imageData[nidx + (xi*3)];
                
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
        ef->x = (int)x;
        ef->y = (int)y;
        ef->copyImageData(ef->x, ef->y);
        ef->speed = pixelsPerSecond;
        ef->disableWhenDone = disableWhenDone;
        ef->direction = position;
        lock();
        int32_t t = 1000/pixelsPerSecond;
        if (t == 0) {
            t = 1;
        }
        setRunningEffect(ef, t);
    }
}
uint8_t *PixelOverlayModel::getOverlayBuffer() {
    if (!overlayBuffer)
        overlayBuffer = (uint8_t*)malloc(block->channelCount);
    return overlayBuffer;
}




void PixelOverlayModel::getDataJson(Json::Value &v) {
    for (int c = 0; c < block->channelCount; c++) {
        unsigned char i = chanDataMap[pixelMap[block->startChannel + c - 1]];
        v.append(i);
    }
}
bool PixelOverlayModel::isLocked() {
    return block->isLocked;
}
void PixelOverlayModel::lock(bool l) {
    block->isLocked = l ? 1 : 0;
}

int PixelOverlayModel::getStartChannel() const {
    return block->startChannel;
}
int PixelOverlayModel::getChannelCount() const {
    return block->channelCount;
}
bool PixelOverlayModel::isHorizontal() const {
    return block->orientation == 'H';
}
int PixelOverlayModel::getNumStrings() const {
    return block->stringCount;
}
int PixelOverlayModel::getStrandsPerString() const {
    return block->strandsPerString;
}
std::string PixelOverlayModel::getStartCorner() const {
    return block->startCorner;
}
void PixelOverlayModel::toJson(Json::Value &result) {
    result["Name"] = getName();
    result["StartChannel"] = getStartChannel();
    result["ChannelCount"] = getChannelCount();
    result["Orientation"] = isHorizontal() ? "horizontal" : "vertical";
    result["StartCorner"] = getStartCorner();
    result["StringCount"] = getNumStrings();
    result["StrandsPerString"] = getStrandsPerString();
}


int32_t PixelOverlayModel::updateRunningEffects() {
    std::unique_lock<std::mutex> l(effectLock);
    if (runningEffect) {
        int32_t v = runningEffect->update();
        if (v == 0) {
            delete runningEffect;
            runningEffect = nullptr;
        }
        return v;
    }
    return 0;
}

void PixelOverlayModel::setRunningEffect(RunningEffect *ef, int32_t firstUpdateMS) {
    std::unique_lock<std::mutex> l(effectLock);
    if (runningEffect) {
        delete runningEffect;
        PixelOverlayManager::INSTANCE.removePeriodicUpdate(this);
    }
    runningEffect = ef;
    PixelOverlayManager::INSTANCE.addPeriodicUpdate(firstUpdateMS, this);
}

bool PixelOverlayModel::applyEffect(bool autoEnable, const std::string &effect, const std::vector<std::string> &args) {
    PixelOverlayEffect *pe = PixelOverlayEffect::GetPixelOverlayEffect(effect);
    if (pe) {
        return pe->apply(this, autoEnable, args);
    }
    return false;
}

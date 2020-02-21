#include <fcntl.h>
#include <unistd.h>

#include <vector>

#include "common.h"
#include "log.h"
#include "TmpFileGPIO.h"


TmpFilePinCapabilities::TmpFilePinCapabilities(const std::string &n, uint32_t kg)
: PinCapabilitiesFluent(n, kg) {

    filename = "/tmp/GPIO-";
    filename += n;
}


int TmpFilePinCapabilities::configPin(const std::string& mode,
                                 bool directionOut) const {
    unlink(filename.c_str());
    return 0;
}

bool TmpFilePinCapabilities::getValue() const {
    return FileExists(filename);
}
void TmpFilePinCapabilities::setValue(bool i) const {
    if (i)
        Touch(filename);
    else
        unlink(filename.c_str());
}

class NullTmpFilePinCapabilities : public TmpFilePinCapabilities {
public:
    NullTmpFilePinCapabilities() : TmpFilePinCapabilities("-none-", 0) {}
    virtual const PinCapabilities *ptr() const override { return nullptr; }
};
static NullTmpFilePinCapabilities NULL_TMPFILE_INSTANCE;



static std::vector<TmpFilePinCapabilities> TMPFILE_PINS;


void TmpFilePinCapabilities::Init() {
    for (int x = 1; x <= 20; x++) {
        std::string p = "TF-" + std::to_string(x);
        TMPFILE_PINS.push_back(TmpFilePinCapabilities(p, x));
    }
}
std::vector<std::string> TmpFilePinCapabilities::getPinNames() {
    std::vector<std::string> ret;
    for (auto &a : TMPFILE_PINS) {
        ret.push_back(a.name);
    }
    return ret;
}
const TmpFilePinCapabilities &TmpFilePinCapabilities::getPinByName(const std::string &name) {
    for (auto &a : TMPFILE_PINS) {
        if (a.name == name) {
            return a;
        }
    }
    return NULL_TMPFILE_INSTANCE;
}
const TmpFilePinCapabilities &TmpFilePinCapabilities::getPinByGPIO(int i) {
    for (auto &a : TMPFILE_PINS) {
        if (a.kernelGpio == i) {
            return a;
        }
    }
    return NULL_TMPFILE_INSTANCE;
}
const TmpFilePinCapabilities &TmpFilePinCapabilities::getPinByUART(const std::string &n) {
    return NULL_TMPFILE_INSTANCE;
}


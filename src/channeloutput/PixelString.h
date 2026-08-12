#pragma once
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

#include <cmath>
#include "fpp-json.h"
#include <list>
#include <stdint.h>
#include <string>
#include <vector>

#include "ColorOrder.h"

// Brightness/gamma lookup tables are a pure function of (brightness, gamma) -
// see ReadVirtualString - and it is very common for many ports to carry the
// same settings, a whole megatree being the obvious case.  Handing those a
// single shared table instead of a private copy each keeps the tables the
// frame interleave is walking small enough to stay resident, and drops the
// memory from one table per virtual string to one per distinct setting.
//
// Sharing is also why inversion cannot flip a table in place any more: the
// inverted form is just a different cached table.
//
// Tables live for the life of the process.  They are only ever built during
// output Init and are read-only afterwards, so the lock is uncontended.
class BrightnessLUTCache {
public:
    // brightness is 0-100; inverted returns the complemented table used by a
    // line that idles high.  Never returns null.
    static const uint8_t* get(int brightness, float gamma, bool inverted);

    // The 16 bit form for parts like UCS8903.  Sequence data is always 8 bit
    // per channel, so this is where the extra range comes from: the same
    // brightness/gamma curve evaluated to 65535 instead of 255.  That is the
    // whole point of a 16 bit part - at gamma 2.2 the low 8 bit inputs all
    // round to the same output, and expanding at the wider width keeps them
    // distinct.  Simply duplicating the byte would buy nothing.
    static const uint16_t* getWide(int brightness, float gamma, bool inverted);

    // distinct tables built so far - diagnostics and tests
    static size_t tableCount();
};

class VirtualString {
public:
    VirtualString();
    VirtualString(int receiverType, int receiverNum);
    ~VirtualString() {};

    int channelsPerNode() const;

    bool isSmartReceiver() const;
    bool isFalconV5SmartReceiver() const;

    int startChannel;
    int pixelCount;
    int groupCount;
    int reverse;
    FPPColorOrder colorOrder;
    int startNulls;
    int endNulls;
    int zigZag;
    int brightness;
    float gamma;
    // owned by BrightnessLUTCache and shared with every virtual string that
    // has the same brightness/gamma/inversion - do not write through it
    const uint8_t* brightnessMap;
    // the 16 bit form of the same curve; only set on a 16 bit port
    const uint16_t* brightnessMap16 = nullptr;

    int8_t receiverNum;
    uint8_t leadInCount;
    uint8_t toggleCount;
    uint8_t leadOutCount;
    std::string description;

    int* chMap;
    int chMapCount;
};

class GPIOCommand {
public:
    GPIOCommand(int p, int offset, int tp = 0, int bo = 0) :
        port(p),
        channelOffset(offset),
        type(tp),
        bitOffset(bo) {}
    ~GPIOCommand() {}

    int port;
    int channelOffset;
    int type; // 0 for off, 1 for on
    int bitOffset;
};

class PixelString {
public:
    PixelString(bool supportsSmartReceivers = false);
    ~PixelString();

    int Init(Json::Value config, Json::Value* pinConfig = nullptr);
    void DumpConfig(void);
    void invertOutput();

    // Pixel protocols whose line idles high and pulses low (the TM18xx
    // family).  The bit cell is otherwise identical to ws2811, so a driver
    // that can flip the idle level per port drives them alongside normal
    // strings at no cost - see m_isInverted.
    static bool isInvertedProtocol(const std::string& protocol);

    // TM1814's C1/C2 words: four 6 bit constant current levels in the chip's
    // physical W,R,G,B pin order, then the bitwise complement of all four
    // (the chip refuses to decode the frame without it).  milliAmps is per
    // channel and clamps to the 6.5-38mA the part supports.
    static std::vector<uint8_t> buildTM1814Preamble(float milliAmps);
    static bool protocolNeedsTM1814Preamble(const std::string& protocol);

    // Bytes clocked per colour channel: 1 for the usual 8 bit parts, 2 for the
    // 16 bit ones.  The sequence still supplies 8 bits either way - see
    // BrightnessLUTCache::getWide - so this only changes the wire length.
    static int protocolBytesPerChannel(const std::string& protocol);

    // The bit cell a protocol wants, in ns.  t0/t1 are how long the line is
    // driven for a zero and a one; period is the whole cell.  These are the
    // instants every port on a PRU shares, so a driver can only honour one set
    // at a time - see BBShiftString, which derives the controller's timing
    // from the ports in use and refuses to mix.
    struct Timing {
        int t0Ns;
        int t1Ns;
        int periodNs;
        bool operator==(const Timing& o) const {
            return t0Ns == o.t0Ns && t1Ns == o.t1Ns && periodNs == o.periodNs;
        }
    };
    static Timing protocolTiming(const std::string& protocol);

    // Set the wire length from a count of colour channels.  The licensed
    // output clamps rebuild that count from the (reduced) virtual strings, so
    // it is channels and excludes both the protocol preamble and any 16 bit
    // widening - assigning it straight to m_outputBytes would quietly drop
    // them, so route it through here.
    void setPixelDataChannels(int channels);

    // Drop a preamble the driver cannot honour and shorten the port back to
    // plain pixel data, keeping m_outputBytes and the gpio commands in step.
    // Without this the leading bytes would offset every pixel on the port.
    void dropPreamble();

    int m_portNumber;
    int m_channelOffset;
    // bytes clocked out on this port, including any protocol preamble
    int m_outputBytes;
    uint8_t* m_outputBuffer;

    // Constant bytes the protocol requires ahead of the pixel data - TM1814's
    // C1/C2 constant current words.  Counted in m_outputBytes and parked
    // permanently at the head of m_outputBuffer, so anything that fills the
    // buffer just has to start at pixelDataStart() and the preamble comes
    // along for free.
    std::vector<uint8_t> m_preamble;
    uint8_t* pixelDataStart() const { return m_outputBuffer + m_preamble.size(); }

    std::vector<VirtualString> m_virtualStrings;
    std::vector<GPIOCommand> m_gpioCommands;

    std::vector<int> m_outputMap;
    const uint8_t** m_brightnessMaps;

    enum class ReceiverType {
        None = -1,
        Standard = 0,
        v1 = 1,
        v2 = 2,
        // Falcon differential receivers: FalconV5 is the full bidirectional
        // protocol (config + queries + listen, needs cape listener support);
        // FalconV4 only sends the config packet
        FalconV5 = 3,
        FalconV4 = 4
    };
    ReceiverType smartReceiverType = PixelString::ReceiverType::Standard;
    bool m_isSmartReceiver = false;

    // The line for this port idles high instead of low.  Two independent
    // things set this and they compose by xor:
    //
    //  - the cape declaring "inverted" on the port, which compensates for a
    //    differential pair wired backwards (one batch of K16A-B boards had
    //    this on the second line of the first differential group).  The far
    //    end sees a correct signal, so smart receivers are unaffected.
    //  - the port's protocol being one of the TM18xx family, which really
    //    does want an inverted line all the way to the pixels.  A smart
    //    receiver in that path would see inverted data, so drivers refuse
    //    the combination.
    //
    // A TM18xx string on a backwards-wired port cancels out, which is what
    // the hardware actually does.
    bool m_isInverted = false;
    // 1, or 2 for a 16 bit part.  Set before the virtual strings are read so
    // the wire length and the lookup tables agree.
    int m_bytesPerChannel = 1;
    // set when m_isInverted came from the protocol rather than the cape, so
    // drivers can apply the smart receiver restriction to just that case
    bool m_protocolInverted = false;

    static void AutoCreateOverlayModels(const std::vector<PixelString*>& strings, std::list<std::string>& autoModelNames);

    // returned buffer is owned by the PixelString and reused next frame
    uint8_t* prepareOutput(uint8_t* channelData);

private:
    void SetupMap(int vsOffset, const VirtualString& vs);
    void FlipPixels(int offset1, int offset2, int chanCount);
    void DumpMap(const char* msg);

    int ReadVirtualString(Json::Value& vsc, VirtualString& vs) const;
    void AddVirtualString(const VirtualString& vs);
    void AddNullPixelString();

    Json::Value m_pinConfig;
};

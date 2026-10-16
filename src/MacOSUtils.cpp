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

#include <getopt.h>
#include <algorithm>
#include <cstddef>
#include <map>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#include "settings.h"

typedef uint32_t UInt32;
typedef UInt32 AudioObjectID;
typedef UInt32 AudioDeviceID;
typedef UInt32 OSStatus;

constexpr uint32_t kAudioObjectSystemObject = 1;

struct AudioObjectPropertyAddress {
    uint32_t mSelector;
    uint32_t mScope;
    uint32_t mElement;
};
struct AudioBuffer {
    UInt32 mNumberChannels;
    UInt32 mDataByteSize;
    void* mData;
};
struct AudioBufferList {
    UInt32 mNumberBuffers;
    AudioBuffer mBuffers[1]; // this is a variable length array of mNumberBuffers elements
};

typedef struct AudioObjectPropertyAddress AudioObjectPropertyAddress;
typedef UInt32 AudioDevicePropertyID;

constexpr uint32_t kAudioHardwarePropertyDevices = 'dev#';
constexpr uint32_t kAudioHardwarePropertyDefaultInputDevice = 'dIn ';
constexpr uint32_t kAudioHardwarePropertyDefaultOutputDevice = 'dOut';
constexpr uint32_t kAudioHardwarePropertyDefaultSystemOutputDevice = 'sOut';
constexpr uint32_t kAudioHardwarePropertyTranslateUIDToDevice = 'uidd';
constexpr uint32_t kAudioHardwarePropertyMixStereoToMono = 'stmo';
constexpr uint32_t kAudioHardwarePropertyPlugInList = 'plg#';
constexpr uint32_t kAudioHardwarePropertyTranslateBundleIDToPlugIn = 'bidp';
constexpr uint32_t kAudioHardwarePropertyTransportManagerList = 'tmg#';
constexpr uint32_t kAudioHardwarePropertyTranslateBundleIDToTransportManager = 'tmbi';
constexpr uint32_t kAudioHardwarePropertyBoxList = 'box#';
constexpr uint32_t kAudioHardwarePropertyTranslateUIDToBox = 'uidb';
constexpr uint32_t kAudioHardwarePropertyClockDeviceList = 'clk#';
constexpr uint32_t kAudioHardwarePropertyTranslateUIDToClockDevice = 'uidc';
constexpr uint32_t kAudioHardwarePropertyProcessIsMain = 'main';
constexpr uint32_t kAudioHardwarePropertyIsInitingOrExiting = 'inot';
constexpr uint32_t kAudioHardwarePropertyUserIDChanged = 'euid';
constexpr uint32_t kAudioHardwarePropertyProcessIsAudible = 'pmut';
constexpr uint32_t kAudioHardwarePropertySleepingIsAllowed = 'slep';
constexpr uint32_t kAudioHardwarePropertyUnloadingIsAllowed = 'unld';
constexpr uint32_t kAudioHardwarePropertyHogModeIsAllowed = 'hogr';
constexpr uint32_t kAudioHardwarePropertyUserSessionIsActiveOrHeadless = 'user';
constexpr uint32_t kAudioHardwarePropertyServiceRestarted = 'srst';
constexpr uint32_t kAudioHardwarePropertyPowerHint = 'powh';
constexpr uint32_t kAudioObjectPropertyScopeGlobal = 'glob';
constexpr uint32_t kAudioObjectPropertyScopeInput = 'inpt';
constexpr uint32_t kAudioObjectPropertyScopeOutput = 'outp';
constexpr uint32_t kAudioObjectPropertyScopePlayThrough = 'ptru';
constexpr uint32_t kAudioObjectPropertyElementMain = 0;

constexpr uint32_t kAudioDevicePropertyDeviceName = 'name';
constexpr uint32_t kAudioHardwareServiceDeviceProperty_VirtualMainVolume = 'vmvc';
constexpr uint32_t kAudioDevicePropertyStreamConfiguration = 'slay';

extern "C" {
extern OSStatus
AudioObjectGetPropertyDataSize(AudioObjectID inObjectID,
                               const AudioObjectPropertyAddress* inAddress,
                               UInt32 inQualifierDataSize,
                               const void* __nullable inQualifierData,
                               UInt32* outDataSize);
extern OSStatus
AudioObjectGetPropertyData(AudioObjectID inObjectID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* __nullable inQualifierData,
                           UInt32* ioDataSize,
                           void* outData);
extern OSStatus
AudioObjectSetPropertyData(AudioObjectID inObjectID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* __nullable inQualifierData,
                           UInt32 inDataSize,
                           const void* inData);
extern OSStatus AudioDeviceGetPropertyInfo(AudioDeviceID inDevice, UInt32 inChannel,
                                           bool isInput, AudioDevicePropertyID inPropertyID,
                                           UInt32* outSize, bool* outWritable);
extern OSStatus AudioDeviceGetProperty(AudioDeviceID inDevice, UInt32 inChannel,
                                       bool isInput, AudioDevicePropertyID inPropertyID,
                                       UInt32* ioPropertyDataSize, void* outPropertyData);
}

constexpr OSStatus kAudioNoErr = 0;

bool hasOutputChannels(AudioDeviceID id) {
    UInt32 outputChannelCount = 0;
    // CoreAudio leaves the size untouched when a call fails (a device unplugged
    // since the list was read), so it starts at 0; and the list is walked only
    // as far as the bytes actually returned, not as far as mNumberBuffers
    // claims, which is the driver's word and was read past the allocation.
    UInt32 sz = 0;
    if (AudioDeviceGetPropertyInfo(id, 0, false,
                                   kAudioDevicePropertyStreamConfiguration,
                                   &sz, nullptr) != kAudioNoErr) {
        return false;
    }
    if (sz > offsetof(AudioBufferList, mBuffers)) {
        AudioBufferList* bufferList = (AudioBufferList*)malloc(sz);
        if (bufferList == nullptr) {
            return false;
        }
        if (AudioDeviceGetProperty(id, 0, false,
                                   kAudioDevicePropertyStreamConfiguration,
                                   &sz, bufferList) == kAudioNoErr &&
            sz > offsetof(AudioBufferList, mBuffers)) {
            size_t fits = (sz - offsetof(AudioBufferList, mBuffers)) / sizeof(AudioBuffer);
            size_t count = std::min((size_t)bufferList->mNumberBuffers, fits);
            for (size_t c = 0; c < count; c++) {
                outputChannelCount += bufferList->mBuffers[c].mNumberChannels;
            }
        }
        free(bufferList);
    }
    return outputChannelCount > 0;
}

// The volume is set from the media worker and read from the HTTP threads, and
// both look the device up here.
static std::mutex OUT_DEVICE_LOCK;
static std::map<std::string, AudioDeviceID> OUT_DEVICE_MAP;

static AudioDeviceID getAudioDeviceID(const char* deviceName) {
    std::string dn = deviceName;
    std::lock_guard<std::mutex> lock(OUT_DEVICE_LOCK);
    if (OUT_DEVICE_MAP.find(dn) == OUT_DEVICE_MAP.end()) {
        UInt32 propertySize = 0;
        AudioObjectID defaultID = kAudioObjectSystemObject;

        // Get address for all hardware devices
        AudioObjectPropertyAddress devicesAddress = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };

        // Retreive property size within devices address
        std::vector<AudioDeviceID> ids;
        if (AudioObjectGetPropertyDataSize(defaultID, &devicesAddress, 0, nullptr, &propertySize) == kAudioNoErr) {
            ids.resize(propertySize / sizeof(AudioObjectID));
        }

        // Populate ids from devices address.  The list can shrink between the
        // two calls, so keep only what was written.
        if (!ids.empty()) {
            propertySize = ids.size() * sizeof(AudioObjectID);
            if (AudioObjectGetPropertyData(defaultID, &devicesAddress, 0, nullptr, &propertySize, ids.data()) == kAudioNoErr) {
                ids.resize(std::min(ids.size(), (size_t)(propertySize / sizeof(AudioObjectID))));
            } else {
                ids.clear();
            }
        }

        for (size_t i = 0; i < ids.size(); i++) {
            if (hasOutputChannels(ids[i])) {
                char name[256];
                UInt32 nameSize = sizeof(name) - 1;

                AudioObjectPropertyAddress deviceNameAddress = {
                    kAudioDevicePropertyDeviceName,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };

                // Get device name from id.  Nothing promises a terminator
                // within the size given, and a failed call writes nothing.
                if (AudioObjectGetPropertyData(ids[i], &deviceNameAddress, 0, nullptr, &nameSize, (char*)name) != kAudioNoErr) {
                    continue;
                }
                name[std::min(nameSize, (UInt32)(sizeof(name) - 1))] = 0;
                // Check if names match
                if (strcmp(deviceName, (char*)name) == 0) {
                    OUT_DEVICE_MAP[dn] = ids[i];
                    return ids[i];
                }
            }
        }
        // Get address for default output device
        AudioObjectPropertyAddress deviceAddress = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        UInt32 didSize = sizeof(defaultID);
        if (AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject), &deviceAddress, 0, nullptr, &didSize, &defaultID) != kAudioNoErr) {
            // not cached, so the next call asks again rather than keeping the
            // system object as this name's device until fppd restarts
            return kAudioObjectSystemObject;
        }
        OUT_DEVICE_MAP[dn] = defaultID;
    }
    return OUT_DEVICE_MAP[dn];
}

void MacOSSetVolume(int fvol) {
    std::string dev = getSetting("AudioOutput", "--System Default--");
    AudioDeviceID id;
    if (dev != "--System Default--") {
        id = getAudioDeviceID(dev.c_str());
    } else {
        id = getAudioDeviceID("");
    }

    float volume = fvol;
    volume /= 100.0f;

    UInt32 volumeSize = sizeof(volume);
    AudioObjectPropertyAddress volumeAddress = {
        kAudioHardwareServiceDeviceProperty_VirtualMainVolume,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    // Set new volume level
    AudioObjectSetPropertyData(id, &volumeAddress, 0, nullptr, volumeSize, &volume);
}

int MacOSGetVolume() {
    std::string dev = getSetting("AudioOutput", "--System Default--");
    AudioDeviceID id;
    if (dev != "--System Default--") {
        id = getAudioDeviceID(dev.c_str());
    } else {
        id = getAudioDeviceID("");
    }
    float volume = 0;

    UInt32 volumeSize = sizeof(volume);
    AudioObjectPropertyAddress volumeAddress = {
        kAudioHardwareServiceDeviceProperty_VirtualMainVolume,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    // Set new volume level
    AudioObjectGetPropertyData(id, &volumeAddress, 0, nullptr, &volumeSize, &volume);
    return volume * 100;
}

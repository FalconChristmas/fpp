
#include "fpp-pch.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "settings.h"


typedef uint32_t UInt32;
typedef UInt32 AudioObjectID;
typedef UInt32 AudioDeviceID;
typedef UInt32 OSStatus;

constexpr uint32_t kAudioObjectSystemObject = 1;

struct  AudioObjectPropertyAddress
{
    uint32_t mSelector;
    uint32_t mScope;
    uint32_t mElement;
};
typedef struct AudioObjectPropertyAddress   AudioObjectPropertyAddress;

constexpr uint32_t kAudioHardwarePropertyDevices                               = 'dev#';
constexpr uint32_t kAudioHardwarePropertyDefaultInputDevice                    = 'dIn ';
constexpr uint32_t kAudioHardwarePropertyDefaultOutputDevice                   = 'dOut';
constexpr uint32_t kAudioHardwarePropertyDefaultSystemOutputDevice             = 'sOut';
constexpr uint32_t kAudioHardwarePropertyTranslateUIDToDevice                  = 'uidd';
constexpr uint32_t kAudioHardwarePropertyMixStereoToMono                       = 'stmo';
constexpr uint32_t kAudioHardwarePropertyPlugInList                            = 'plg#';
constexpr uint32_t kAudioHardwarePropertyTranslateBundleIDToPlugIn             = 'bidp';
constexpr uint32_t kAudioHardwarePropertyTransportManagerList                  = 'tmg#';
constexpr uint32_t kAudioHardwarePropertyTranslateBundleIDToTransportManager   = 'tmbi';
constexpr uint32_t kAudioHardwarePropertyBoxList                               = 'box#';
constexpr uint32_t kAudioHardwarePropertyTranslateUIDToBox                     = 'uidb';
constexpr uint32_t kAudioHardwarePropertyClockDeviceList                       = 'clk#';
constexpr uint32_t kAudioHardwarePropertyTranslateUIDToClockDevice             = 'uidc';
constexpr uint32_t kAudioHardwarePropertyProcessIsMain                         = 'main';
constexpr uint32_t kAudioHardwarePropertyIsInitingOrExiting                    = 'inot';
constexpr uint32_t kAudioHardwarePropertyUserIDChanged                         = 'euid';
constexpr uint32_t kAudioHardwarePropertyProcessIsAudible                      = 'pmut';
constexpr uint32_t kAudioHardwarePropertySleepingIsAllowed                     = 'slep';
constexpr uint32_t kAudioHardwarePropertyUnloadingIsAllowed                    = 'unld';
constexpr uint32_t kAudioHardwarePropertyHogModeIsAllowed                      = 'hogr';
constexpr uint32_t kAudioHardwarePropertyUserSessionIsActiveOrHeadless         = 'user';
constexpr uint32_t kAudioHardwarePropertyServiceRestarted                      = 'srst';
constexpr uint32_t kAudioHardwarePropertyPowerHint                             = 'powh';
constexpr uint32_t kAudioObjectPropertyScopeGlobal         = 'glob';
constexpr uint32_t kAudioObjectPropertyScopeInput          = 'inpt';
constexpr uint32_t kAudioObjectPropertyScopeOutput         = 'outp';
constexpr uint32_t kAudioObjectPropertyScopePlayThrough    = 'ptru';
constexpr uint32_t kAudioObjectPropertyElementMain            = 0;

constexpr uint32_t kAudioDevicePropertyDeviceName                          = 'name';
constexpr uint32_t kAudioHardwareServiceDeviceProperty_VirtualMainVolume     = 'vmvc';

extern "C" {
extern OSStatus
AudioObjectGetPropertyDataSize(AudioObjectID                       inObjectID,
                               const AudioObjectPropertyAddress*   inAddress,
                               UInt32                              inQualifierDataSize,
                               const void* __nullable              inQualifierData,
                               UInt32*                             outDataSize);
extern OSStatus
AudioObjectGetPropertyData(AudioObjectID                       inObjectID,
                           const AudioObjectPropertyAddress*   inAddress,
                           UInt32                              inQualifierDataSize,
                           const void* __nullable              inQualifierData,
                           UInt32*                             ioDataSize,
                           void*                               outData);
extern OSStatus
AudioObjectSetPropertyData(AudioObjectID                       inObjectID,
                           const AudioObjectPropertyAddress*   inAddress,
                           UInt32                              inQualifierDataSize,
                           const void* __nullable              inQualifierData,
                           UInt32                              inDataSize,
                           const void*                         inData);
}

static AudioDeviceID getAudioDeviceID(const char * deviceName) {
    UInt32 propertySize;
    int numberOfDevices = 0;
    AudioObjectID defaultID = kAudioObjectSystemObject;
  
    // Get address for all hardware devices
    AudioObjectPropertyAddress devicesAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
  
    // Retreive property size within devices address
    AudioObjectGetPropertyDataSize(defaultID, &devicesAddress, 0, nullptr, &propertySize);
  
    numberOfDevices = propertySize / sizeof(AudioObjectID);
    AudioDeviceID ids[numberOfDevices];
  
    // Populate ids from devices address
    AudioObjectGetPropertyData(defaultID, &devicesAddress, 0, nullptr, &propertySize, ids);
  
    for (int i = 0; i < numberOfDevices; i++) {
        UInt32 nameSize = 256;
        char name[nameSize];
    
        AudioObjectPropertyAddress deviceNameAddress = {
            kAudioDevicePropertyDeviceName,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
    
        // Get device name from id
        AudioObjectGetPropertyData(ids[i], &deviceNameAddress, 0, nullptr, &nameSize, (char *) name);
        // Check if names match
        if (strcmp(deviceName, (char *) name) == 0) {
            return ids[i];
        }
    }
    // Get address for default output device
    AudioObjectPropertyAddress deviceAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    unsigned int didSize = sizeof(defaultID);
    AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject), &deviceAddress, 0, nullptr, &didSize, &defaultID);
    return defaultID;
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
    AudioObjectPropertyAddress volumeAddress =  {
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
    AudioObjectPropertyAddress volumeAddress =  {
        kAudioHardwareServiceDeviceProperty_VirtualMainVolume,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };
    
    // Set new volume level
    AudioObjectGetPropertyData(id, &volumeAddress, 0, nullptr, &volumeSize, &volume);
    return volume * 100;
}

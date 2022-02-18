
#include "fpp-pch.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>

#include "settings.h"


static AudioDeviceID getAudioDeviceID(const char * deviceName) {
    UInt32 propertySize;
    int numberOfDevices = 0;
    AudioObjectID defaultID = kAudioObjectSystemObject;
  
    // Get address for all hardware devices
    AudioObjectPropertyAddress devicesAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
  
    // Retreive property size within devices address
    AudioObjectGetPropertyDataSize(defaultID, &devicesAddress, 0, nil, &propertySize);
  
    numberOfDevices = propertySize / sizeof(AudioObjectID);
    AudioDeviceID ids[numberOfDevices];
  
    // Populate ids from devices address
    AudioObjectGetPropertyData(defaultID, &devicesAddress, 0, nil, &propertySize, ids);
  
    for (int i = 0; i < numberOfDevices; i++) {
        UInt32 nameSize = 256;
        char name[nameSize];
    
        AudioObjectPropertyAddress deviceNameAddress = {
            kAudioDevicePropertyDeviceName,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
    
        // Get device name from id
        AudioObjectGetPropertyData(ids[i], &deviceNameAddress, 0, nil, &nameSize, (char *) name);
        // Check if names match
        if (strcmp(deviceName, (char *) name) == 0) {
            return ids[i];
        }
    }
    // Get address for default output device
    AudioObjectPropertyAddress deviceAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
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
        kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    };
    
    // Set new volume level
    AudioObjectSetPropertyData(id, &volumeAddress, 0, nil, volumeSize, &volume);
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
        kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMaster
    };
    
    // Set new volume level
    AudioObjectGetPropertyData(id, &volumeAddress, 0, nil, &volumeSize, &volume);
    return volume * 100;
}

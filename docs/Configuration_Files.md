## Falcon Player Configuration File Formats

The global setting file for FPP is located at

```
/home/fpp/media/setting
```

The settings file contains a list of key=value pairs, one per line. This
file is used to store the one-off non-repetitive configuration settings. The
configuration for E1.31/DDP Outputs, Playlists, Schedules, are stored in separate config
files.

Field Definitions:
key = value

Sample Data:

```
HostName = "FPP"
fppMode = "player"
volume = "64"
```

Setting deifnitions are held in www/settings.json this file documents the structure of the settings definitions:

[Setting Definitions Explained](/Settings_configurations.md)

## Schedule Configuration File

The Schedule settings are stored in a JSON located at:

```
/home/fpp/media/setting/schedule.json
```

The schedule JSON file contains a JSON array of the schedule entries.

#### Field definitions:

enabled         - 0 or 1 whether the shedule entry is inactive or active
playlist        - The name of the playlist to play on this schedule
day             - 0-6 - schedule runs on a single day of the week
7   - run the schedule every day
8   - run weekdays (Mon-Fri)
9   - run weekends (Sat-Sun)
10  - run Monday, Wednesday, Friday
11  - run Tuesday, Thursday
12  - run Sunday through Thursday
13  - run Friday and Saturday
14  - run Odd Number Days
15  - run Even Number Days
startTime       - Schedule Start Time
startTimeOffset - Start Time Offset, Used mostly with Dawn/Dusk Timing
endTime         - Schedule End Time
endTimeOffset   - Schedule End Offset, Used mostly with Dawn/Dusk Timing
repeat          - 0 or 1 to repeat playlist during the scheduled time slot
startDate       - Schedule Start Date
endDate         - Schedule End Date
stopType        - 0 - Graceful, Play until End of Sequence
1 - Hard Start, Stop immediately.
2 - Graceful Loop, Play through All Playlist Sequences, then stop.

#### Sample Data:

```json
[
    {
        "enabled": 0,
        "sequence": 0,
        "playlist": "Main",
        "day": 7,
        "startTime": "17:00:00",
        "startTimeOffset": 0,
        "endTime": "12:00:00",
        "endTimeOffset": 0,
        "repeat": 1,
        "startDate": "2020-12-31",
        "endDate": "2020-12-31",
        "stopType": 0
    }
]
```

## Playlist Configuration Files

Each Playlist is stored in an individual JSON file located at:

```
/home/fpp/media/playlists/%Name_OF_Playlist%.json
```

The Playlist JSON files contains overall JSON parameters and then a JSON array of the Playlist entries.

#### Sample Data:

```json
{
    "name": "All Sequences",
    "version": 3,
    "repeat": 0,
    "loopCount": 0,
    "desc": "",
    "mainPlaylist": [
        {
            "type": "both",
            "enabled": 1,
            "playOnce": 0,
            "sequenceName": "Bebe Rexha Count On Christmas.fseq",
            "mediaName": "Bebe Rexha Count On Christmas.mp3",
            "videoOut": "--Default--",
            "duration": 180.975
        }
    ],
    "playlistInfo": {
        "total_duration": 180.975,
        "total_items": 1
    }
}
```

## E1.31/ArtNet/DDP/KiNet Channel Outputs Configuration File

Ethernet based Channel Outputs Configurations are stored in a JSON file located at:

```
/home/fpp/media/config/co-universes.json
```

The 'enabled' parameter enables all the Ethernet based Channel Outputs.

#### Universes Types:

- 0 - E1.31 Multicast
- 1 - E1.31 Unicast
- 2 - ArtNet Multicast
- 3 - ArtNet Unicast
- 4 - DDP Absolute Addressing
- 5 - DDP One Based Addressing
- 6 - KINET V1
- 7 - KINET V2

#### Sample Data:

```json
{
	"channelOutputs": [
		{
			"type": "universes",
			"enabled": 1,
			"startChannel": 1,
			"timeout": 1000,
			"channelCount": -1,
			"universes": [
				{
					"active": 0,
					"description": "TuneTo",
					"type": 4,
					"address": "192.168.1.200",
					"id": 64005,
					"startChannel": 110593,
					"priority": 0,
					"channelCount": 3072
				},
				{
					"active": 0,
					"description": "House Left",
					"type": 4,
					"address": "192.168.1.205",
					"id": 64003,
					"startChannel": 138241,
					"priority": 0,
					"channelCount": 6968
				}
			],
			"interface": "eth0"
		}
	]
}
```

## Pixel Outputs Configuration File

WS28XX Pixel Outputs Configurations are stored in a JSON file located at:

```
/home/fpp/media/config/co-pixelStrings.json
/home/fpp/media/config/co-bbbStrings.json
```

The 'co-pixelStrings.json' file contains the pixel outputs if using a Raspberry Pi and the
'co-bbbStrings.json' file contains the pixel outputs if using a Beaglebone or PocketBeagle.

#### Sample 'co-bbbStrings.json' File Data:

```json
{
	"channelOutputs": [
		{
			"pinoutVersion": "1.x",
			"startChannel": 1,
			"enabled": 1,
			"channelCount": -1,
			"type": "BBB48String",
			"subType": "PB16-EXP",
			"outputCount": 4,
			"outputs": [
				{
					"portNumber": 0,
					"virtualStrings": [
						{
							"nullNodes": 0,
							"description": "Left Upright",
							"startChannel": 138254,
							"reverse": 0,
							"pixelCount": 12,
							"endNulls": 0,
							"groupCount": 0,
							"colorOrder": "RGB",
							"zigZag": 0,
							"brightness": 50,
							"gamma": "2.2"
						},
						{
							"nullNodes": 0,
							"description": "Roof Line",
							"startChannel": 138290,
							"reverse": 0,
							"pixelCount": 67,
							"endNulls": 0,
							"groupCount": 0,
							"colorOrder": "RGB",
							"zigZag": 0,
							"brightness": 50,
							"gamma": "2.2"
						}
					]
				},
				{
					"portNumber": 1,
					"virtualStrings": [
						{
							"nullNodes": 0,
							"description": "RidgeLine",
							"startChannel": 138527,
							"reverse": 0,
							"pixelCount": 101,
							"endNulls": 0,
							"groupCount": 0,
							"colorOrder": "RGB",
							"zigZag": 0,
							"brightness": 100,
							"gamma": "2.2"
						}
					]
				},
				{
					"portNumber": 2,
					"virtualStrings": [
						{
							"nullNodes": 0,
							"description": "MS Bedroom WF",
							"startChannel": 138830,
							"reverse": 0,
							"pixelCount": 44,
							"endNulls": 0,
							"groupCount": 0,
							"colorOrder": "RGB",
							"zigZag": 0,
							"brightness": 50,
							"gamma": "2.2"
						}
					]
				},
				{
					"portNumber": 3,
					"virtualStrings": [
						{
							"nullNodes": 0,
							"description": "Guest Bedroom WF",
							"startChannel": 138962,
							"reverse": 0,
							"pixelCount": 44,
							"endNulls": 0,
							"groupCount": 0,
							"colorOrder": "RGB",
							"zigZag": 0,
							"brightness": 50,
							"gamma": "2.2"
						}
					]
				}
			],
			"serialInUse": true
		},
		{
			"outputs": [
				{
					"startChannel": 138241,
					"channelCount": 16,
					"outputNumber": 0,
					"outputType": "DMX"
				},
				{
					"startChannel": 138249,
					"channelCount": 16,
					"outputNumber": 1,
					"outputType": "DMX"
				}
			],
			"channelCount": 16,
			"startChannel": 1,
			"enabled": 1,
			"type": "BBBSerial",
			"subType": "DMX",
			"device": "PB16-EXP"
		}
	]
}
```

## Model Overlays Configuration File

The Model Overlays Configuration File is a JSON file located at:

```
/home/fpp/media/config/model-overlays.json
```

The Model Overlays JSON file contains a JSON array of the models entries.
The model entries data is used by Display Testing and Effects Overlays.
This file is generated by xLights, if the Upload Models checkbox is enabled in FPP Connect.

#### Sample Data:

```json
{
   "models" : [
      {
         "Name" : "Matrix",
         "StartCorner" : "TL",
         "ChannelCount" : 110592,
         "Orientation" : "horizontal",
         "StartChannel" : 1,
         "StringCount" : 192,
         "ChannelCountPerNode" : 3,
         "StrandsPerString" : 1
      },
      {
         "Name" : "RidgeLine",
         "StartCorner" : "BL",
         "ChannelCount" : 303,
         "Orientation" : "horizontal",
         "StartChannel" : 138528,
         "StringCount" : 3,
         "ChannelCountPerNode" : 3,
         "StrandsPerString" : 1
      },
      {
         "Name" : "Spider_1",
         "ChannelCount" : 144,
         "Orientation" : "custom",
         "StartChannel" : 141405,
         "StringCount" : 1,
         "ChannelCountPerNode" : 3,
         "StrandsPerString" : 1,
         "data" : ",,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,20,,,,,,,,,,,,,,,,,,,,;,,,,,,,,16,,,,,,,,,,,,,,29,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,34,,,,,,,;,,,,,,,,,,,,21,,,,,,,,,,,,,,,,,,,,;,,,,,,,,15,,,,,,,23,,,26,,,,30,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,33,,,,,,,;,,,,,,,,,17,,,19,,,24,,25,,,,,28,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,27,,,,,,35,,,,,,,;,,,,,,,,,,,,,22,,,,,,,31,,,,,,,,,,,,;,,,,,,,,,,14,,,,,,,,,,,,,32,,,,,,,,,;,,,,,,,,,,,,18,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,36,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,12,,,9,,13,,,,,,,,,,,41,,37,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,40,,,,,;,,,,10,,,,,,,8,,,,,,,,,,,,,,,,,,38,,,;,,,,,,,,,4,,,,,,,,,,,,,46,,,,,,,,,,;,,11,,,,,,,,,,,,,,,,,,,,,,42,,,,,,,,;,,,,,,,,,,,,3,,,,,,,,,47,,,,,,,,,39,,;,,,,,,,7,,,,,,,,,,,,,,,,,,,45,,,,,,;,,,,,,,,,,,,,,2,,,1,,48,,,,,,,,,,,,,;,,,,,,5,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,43,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,6,,,,,,,,,,,,,,,,,,,,,,,,44,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,",
         "StartCorner" : "BL"
      },
      {
         "Name" : "Tombstone_1",
         "ChannelCount" : 180,
         "Orientation" : "custom",
         "StartChannel" : 139953,
         "StringCount" : 1,
         "ChannelCountPerNode" : 3,
         "StrandsPerString" : 1,
         "data" : ",,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,24,,,,,,,,,23,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,25,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,22,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,26,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,21,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,27,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,20,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,35,,,,,,36,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,37,,,,,,,,,,,49,,,,,,,,50,,,,51,,,,52,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,38,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,53,,,,,,,,,,,,,,,,,;,,,,,,,,28,,,,,,,,,,,34,,,,,,,,,,,,,,,,,,,,,48,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,57,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,39,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,19,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,54,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,33,,,,,40,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,41,,,,,,,,,,,,47,,,,,,,,,,,,56,,,,55,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,58,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,42,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,32,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,29,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,46,,,,,,,,59,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,43,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,18,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,31,,,,,,,,,,,,,44,,,,,,,,45,,,,,,,,60,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,30,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,17,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,1,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,16,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,2,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,15,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,3,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,14,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,4,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,13,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,5,,,,,,,,,,6,,,,,,,,,,7,,,,,,,,,8,,,,,,,,,,9,,,,,,,,,10,,,,,,,,,,11,,,,,,,,12,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,;,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,",
         "StartCorner" : "BL"
      },
      {
         "Name" : "TuneTo",
         "StartCorner" : "TL",
         "ChannelCount" : 3072,
         "Orientation" : "horizontal",
         "StartChannel" : 110593,
         "StringCount" : 16,
         "ChannelCountPerNode" : 3,
         "StrandsPerString" : 1
      }
   ]
}
```


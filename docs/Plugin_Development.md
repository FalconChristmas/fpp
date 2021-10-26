# Developing a Plugin for FPP

The FPP Master Plugin list JSON is downloaded the [FPP's github](https://github.com/FalconChristmas/fpp-pluginList/blob/master/pluginList.json).

The Plugin's Repository needs to contain a 'pluginInfo.json' with all the Plugin's configuration information. This is read by FPP to setup the Plugin.

#### Sample 'pluginInfo.json'
```
{
	"repoName": "fpp-brightness",
	"name": "Brightness Control Plugin for FPP",
	"author": "Daniel Kulp (dkulp)",
	"description": "This plugin allows dynamic/real time control of the brightness of the display.",
	"homeURL": "https://github.com/FalconChristmas/fpp-brightness",
	"srcURL": "https://github.com/FalconChristmas/fpp-brightness.git",
	"bugURL": "https://github.com/FalconChristmas/fpp-brightness/issues",
	"allowUpdates": 1,
	"versions": [
		{
				"minFPPVersion": "5.0",
				"maxFPPVersion": "0",
				"branch": "master",
				"sha": ""
		}
	]
}

```
## Plugin Installation

FPP will GIT clone the 'srcURL' from the 'pluginInfo.json' file when the plugin is installed. 
The "%PLUGIN_FOLDER%/scripts/fpp_install.sh" script will be run after the git clone to setup the plugin.

## Plugin Types
FPP supports Script and C++ based Plugins.

### Script Plugin
Script based Plugin are PHP or bash(.sh) files that are called by FPP commands. 
The "%PLUGIN_FOLDER%/commands/descriptions.json" contains the list of commands that will be added to FPP and what script to execute when the command is triggered.

#### Sample Script Plugin: https://github.com/FalconChristmas/fpp-plugin-Template/

### C++ Plugin
C++ based Plugin are C++ libraries(.so files) that are linked into the FPPD daemon. 
The plugins needs a makefile with the source code file locations and the 'fpp_install.sh' script need to run make to compile the plugin's '.so' file. 
The '.so' file then be loaded by the FPPD daemon on restart. The "Plugin.h" file is the base class that each plugin must sub-class.

#### Sample C++ Plugin: https://github.com/FalconChristmas/fpp-brightness


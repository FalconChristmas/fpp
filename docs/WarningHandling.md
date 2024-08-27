# Warning Handling

FPP Warnings are displayed and logged by a CPP Handler

Legacy warning handler just passed a warning message to downstream functionality using:

```cpp
WarningHolder::AddWarning(message);
eg
WarningHolder::AddWarning("MQTT Disconnected");
```

As of FPP v8 this functionality has been extended to allow the assigning of a unique ID to each warning and a data payload to pass useful current variables to the warning record:

```cpp
WarningHolder::AddWarning(id, message, datapayload);
eg
WarningHolder::AddWarning(2, EXCESSIVE_LOG_LEVEL_WARNING, datapayload);
```

To keep track of the warnings a definitions file has been created which needs to be updated when I new unique id is assigned:   www/warnings-definitions.json

This file allows the warning to be assigned to a warning group which affects how it is displayed in the UI and it also allows for configuration of how the warning is displayed in the UI.

There are 3 possible display modes

1. Basic - Simple Help text defined in the definitions file is displayed in the modal UI popup
2. MD File - A modal Popup in the UI displays the content a md file which has been created for that warning
3. PHP - Modal Popup display the content from a php based warning helper file - this allows for more complex warning help pages which want to display dynamic content based on the data payload passed across in the warning creation

## warnings-defintions.json

This file has a simple structure which allows the definitio of warninggroups and the warnings themselves and takes the format:

```json
{
    "WarningGroups": {
        "Network_Warnings": {
            "fa-icon": "network-wired"
        },
        "FPPD_Warnings": {
            "fa-icon": "feather"
        }
    },
    "Warnings": [
        {
            "id": 1,
            "Title": "FPPD Has Crashed.",
            "HelpTxt": "",
            "HelpPageType": null,
            "WarningGroup": "FPPD_Warnings"
        },
        {
            "id": 2,
            "Title": "Log Level Set to Excessive.",
            "HelpTxt": "",
            "HelpPageType": "md",
            "WarningGroup": "Log_Warnings"
        },
        {
            "id": 3,
            "Title": "Log Level Set to Debug.",
            "HelpTxt": "",
            "HelpPageType": "php",
            "WarningGroup": "Log_Warnings"
        }
    ]
}
```

The warning attributes are defined as:

* id - unique int assigned to the warning - just use next available in sequeunce
* Title - generic title for this warning type (this is static and different to the message which can be parsed to the Handler on warning creation)
* WarningGroup - assign the warning to one of the groups defined at beginning of definitions file
* HelpTxt - basic string of warning help text which will be displayed if no MD or php warning help file defined
* HelpPageType - this is the type of warning help page - needs to be either "md" or "php" depending on the extension of the file in the www/help/warning-helpers folder

Warning help pages in www/help/warning-helpers need to have naming format of:

```
warning-<id>.md or warning-<id>.php
```


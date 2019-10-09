<script>
/////////////////////////////////////////////////////////////////////////////
//Base Class

class OtherBase {
    constructor (name="Base-Type", friendlyName="Base Name", maxChannels=512, fixedChans=false, config = {}) {
        this._typeName = name;
        this._typeFriendlyName = friendlyName;
        this._maxChannels = maxChannels;
        this._fixedChans = fixedChans;
        this._config = config;
    }
    
    PopulateHTMLRow(config) {
        var results = "";
        return results;
    }

    AddNewRow() {
        return this.PopulateHTMLRow(this._config);
    }

    GetOutputConfig(result, cell) {
        return result;
    }

    SetDefaults(row) {
        row.find("td input.count").val(this._maxChannels);
        if(this._fixedChans)
            row.find("td input.count").prop('disabled', true);
    }

    CanAddNewOutput() {
        return true;
    }

    get typeName () {
        return this._typeName;
    }

    get typeFriendlyName () {
        return this._typeFriendlyName;
    }

    get maxChannels () {
        return this._maxChannels;
    }

    get fixedChans () {
        return this._fixedChans;
    }
}

/////////////////////////////////////////////////////////////////////////////
//Base Class for outputs with a "Port" device such as serial and SPI
class OtherBaseDevice extends OtherBase {
    constructor (name="Base-Type-Device", friendlyName="Base Device Name", maxChannels=512, fixedChans=false, devices=["No Devices"], config = {}) {
        config.device = "";
        super(name, friendlyName, maxChannels, fixedChans, config)
        this._devices = devices;
    }
    
    PopulateHTMLRow(config) {
        var results = "";
        results += DeviceSelect(this._devices, config.device);
        return results;
    }

    GetOutputConfig(result, cell) {
        var device = cell.find("select.device").val();

        if (device == "")
            return "";

        result.device = device;
        return result;
    }

    CanAddNewOutput() {
        if (this._devices[0] == "No Devices" || Object.keys(this._devices).length == 0)
            return false;
        return true;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Misc. Support functions
function CreateSelect(optionArray = ["No Options"], currentValue, selectTitle, dropDownTitle, selectClass) {
	var result = selectTitle+": <select class='"+selectClass+"'>";

	if (currentValue == "")
		result += "<option value=''>"+dropDownTitle+"</option>";

	var found = 0;
	for (var key in optionArray) {
		result += "<option value='" + key + "'";
	
		if (currentValue == key) {
			result += " selected";
			found = 1;
		}

		result += ">" + optionArray[key] + "</option>";
	}

	if ((currentValue != '') &&
		(found == 0)) {
		result += "<option value='" + currentValue + "'>" + currentValue + "</option>";
	}
	result += "</select>";

	return result;
}

function DeviceSelect(deviceArray = ["No Devices"], currentValue) {
    return CreateSelect (deviceArray, currentValue, "Port", "-- Port --", "device");
}



/////////////////////////////////////////////////////////////////////////////
// SPI and Serial Devices

var SPIDevices = new Array();
<?
        foreach(scandir("/dev/") as $fileName)
        {
            if (preg_match("/^spidev[0-9]/", $fileName)) {
                echo "SPIDevices['$fileName'] = '$fileName';\n";
            }
        }
?>

var SerialDevices = new Array();
<?
	foreach(scandir("/dev/") as $fileName)
	{
		if ((preg_match("/^ttyS[0-9]+/", $fileName)) ||
			(preg_match("/^ttyACM[0-9]+/", $fileName)) ||
			(preg_match("/^ttyO[0-9]/", $fileName)) ||
			(preg_match("/^ttyS[0-9]/", $fileName)) ||
			(preg_match("/^ttyAMA[0-9]+/", $fileName)) ||
			(preg_match("/^ttyUSB[0-9]+/", $fileName))) {
			echo "SerialDevices['$fileName'] = '$fileName';\n";
		}
	}
?>

var I2CDevices = new Array();
<?
        foreach(scandir("/dev/") as $fileName)
        {
            if (preg_match("/^i2c-[0-9]/", $fileName)) {
                echo "I2CDevices['$fileName'] = '$fileName';\n";
            }
        }
?>



/////////////////////////////////////////////////////////////////////////////
// WS2801 Output via SPI

class SPIWS2801Device extends OtherBaseDevice {
    
    constructor() {
        super("SPI-WS2801", "SPI-WS2801", 1530, false, SPIDevices, {pi36: 0});
    }

    PopulateHTMLRow(config) {
        var result = super.PopulateHTMLRow(config);

        result += " PI36: <input type=checkbox class='pi36'";
        if (config.pi36)
            result += " checked='checked'";

        result += ">";

        return result;
    }

    GetOutputConfig(result, cell) {
        result = super.GetOutputConfig(result, cell);
       
        if (result == "")
            return "";

        var pi36 = 0;

        if (cell.find("input.pi36").is(":checked"))
            pi36 = 1;

        result.pi36 = parseInt(pi36);

        return result;
        }

}

/////////////////////////////////////////////////////////////////////////////
// I2C Output
class I2COutput extends OtherBaseDevice {

    constructor(name="I2C-Output", friendlyName="I2C Output", maxChannels=512, fixedChans=false, minAddr, maxAddr) {
        super(name, friendlyName, maxChannels, fixedChans, I2CDevices, {deviceID: minAddr});
        this._minAddr = minAddr;
        this._maxAddr = maxAddr;
    }

    PopulateHTMLRow(config) {
        var result = super.PopulateHTMLRow(config);

        var addrs = {};
        //build addr array for select option
        for (var addr = this._minAddr; addr <= this._maxAddr; addr++) {
            addrs[addr] = "Hex: 0x"+addr.toString(16)+", Dec: "+addr;
        }

        result += " "+CreateSelect(addrs, config.deviceID, "I2C Address", "Select Address", "addr");

        return result;
    }

    GetOutputConfig(result, cell) {
        result = super.GetOutputConfig(result, cell);
        var addr = cell.find("select.addr").val();
        
        if (result == "" || addr == "")
            return "";
        
        result.deviceID = addr;
        return result;
    }        
}

/////////////////////////////////////////////////////////////////////////////
// Generic SPI Output
class GenericSPIDevice extends OtherBaseDevice {
    
    constructor(name="generic_spi", friendlyName="Generic SPI", maxChannels=16777215, fixedChans=false, devices=SPIDevices, config={speed: 50000}) {
        super(name, friendlyName, maxChannels, fixedChans, devices, config);
    }

    PopulateHTMLRow(config) {
        var result = super.PopulateHTMLRow(config);
        result += " Speed (khz): <input type='number' name='speed' min='1' max='999999' class='speed' value='"+config.speed+"'>";
        return result;
    }

    GetOutputConfig(result, cell) {
        result = super.GetOutputConfig(result, cell);
        var speed = cell.find("select.speed").val();
       
        if (result == "" || speed == "")
            return "";

        result.speed = speed;

        return result;
    }

}


/////////////////////////////////////////////////////////////////////////////
//populate the output devices
var output_modules = [];

//Outputs for all platforms

//Outputs for Raspberry Pi or Beagle
<?
if ($settings['Platform'] == "Raspberry Pi" || $settings['Platform'] == "BeagleBone Black")
{
?>


<?
}
?>
//Outputs for Raspberry Pi
<?
if ($settings['Platform'] == "Raspberry Pi")
{
?>
    output_modules.push(new SPIWS2801Device());
    output_modules.push(new I2COutput("MCP23017", "MCP23017", 16, false, 0x20, 0x27));
    output_modules.push(new GenericSPIDevice());
<?
}
?>


</script>
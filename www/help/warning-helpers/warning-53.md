# Sensor Initialization Failed
FPP could not create or enable a configured sensor source, so its readings (temperatures, voltages, etc.) will not be available. Sensors are normally defined by the installed cape/hat or a sensor configuration; the message names the sensor that failed.

1. Confirm the cape/hat that provides the sensor is correctly installed and detected.
2. Make sure the sensor hardware (e.g. an I2C/IIO device) is connected and powered.
3. Check `fppd.log` for the specific sensor that failed and the underlying error.

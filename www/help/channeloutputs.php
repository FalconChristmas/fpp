<h3>Channel Output Configuration</h3>

<p><b>E1.31</b> - The E1.31 output can drive up to 512 universes over the Pi v2 B's ethernet network interface at 50ms timing or 128 universes on the original A/B/B+.  BeagleBone output should be similar to the Pi v2 B.</p>
<p><b>Falcon Pixelnet/DMX (FPD)</b> - The FPD output can send 32,768 channels out 12 ports configured for the DMX or Pixelnet protocols.  This is currently limited to the first 32768 channels in a sequence.</p>
<p><b>DMX Open</b> - The DMX Open output can send DMX data out generic FTDI-based USB to RS485 dongles.  Other RS485 dongles may work if they support setting arbitrary bit rates.  Support should include the following dongles: Entec Open DMX, LOR, and D-Light along with generic FTDI-based USB to RS485 adapters.</p>
<p><b>DMX Pro</b> - The DMX Pro output can send DMX data out Entec-Pro compatible dongles.  This should include the following dongles: Entec-Pro, Lynx USB dongle w/ DMX firmware, DIYC RPM, DMXking.com, and DIYblinky.com.  If the dongle works using xLights DMX Pro output, it should work in the Falcon Player. </p>
<p><b>RGBMatrix</b> - The RGBMatrix output can drive up to four of the 'P10' style 32x16 RGB LED Panels.  These panels may be wired directly to the Pi's GPIO header or an adapter board may be used to handle the wiring.  The adapter board PCB info and BOM are listed in the Falcon wiki at <a href='http://falconchristmas.com/wiki/index.php/RGB-LED_Adapter'>RGB-LED_Adapter</a>.  Make sure the pinout of your LED panels matches the pinout of the LED port on the adapter board before using one of these with your panel or it will not function properly.</p>
<center>
<b>RGBMatrix Output Connections</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>LED Panel</th><th>Raspberry Pi</th></tr>
<tr><td>GND - Ground</td><td>Pin 3 - GND</td></tr>
<tr><td>R1 - Red 1st bank</td><td>Pin 11 - GPIO 17</td></tr>
<tr><td>G1 - Green 1st bank</td><td>Pin 12 - GPIO 18</td></tr>
<tr><td>B1 - Blue 1st bank</td><td>Pin 15 - GPIO 22</td></tr>
<tr><td>R2 - Red 2nd bank</td><td>Pin 16 - GPIO 23</td></tr>
<tr><td>G2 - Green 2nd bank</td><td>Pin 18 - GPIO 24</td></tr>
<tr><td>B2 - Blue 2nd bank</td><td>Pin 22 - GPIO 25</td></tr>
<tr><td>A - Row address</td><td>Pin 26 - GPIO 7</td></tr>
<tr><td>B - Row address</td><td>Pin 24 - GPIO 8</td></tr>
<tr><td>C - Row address</td><td>Pin 21 - GPIO 9</td></tr>
<tr><td>D - Row address</td><td>(Not Used for 32x16 panels</td></tr>
<tr><td>OE - Neg. Output Enable</td><td>Pin 3 - GPIO 2</td></tr>
<tr><td>CLK - Serial Clock</td><td>Pin 5 - GPIO 3</td></tr>
<tr><td>STR - Strobe row data</td><td>Pin 7 - GPIO 4</td></tr>
</table>
</center>
<p><b>Pixelnet Open</b> - The Pixelnet Open output can send Pixelnet data (one 4096-channel universe) out generic FTDI-based USB to RS485 dongles.</p>
<p><b>Pixelnet Lynx</b> - The Pixelnet Lynx output can send Pixelnet data (one 4096-channel universe) out the Lynx USB dongle w/ Pixelnet firmware.</p>
<p><b>Renard</b> - The Renard output can drive up to 4584 channels at the highest speeds using standard USB to serial dongles.</p>
<p><b>SPI-WS2801</b> - The SPI-WS2801 output can drive one string of WS2801 pixels directly off the Raspberry Pi's SPI port.  The data, clock, and ground lines attach directly to the Pi while power for the pixels is injected from another source.</p>
<center>
<b>SPI-WS2801 Output Connections</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>WS2801 Function</th><th>Raspberry Pi</th></tr>
<tr><td>Clock</td><td>Pin 23 - SCLK</td></tr>
<tr><td>Data</td><td>Pin 19 - MOSI</td></tr>
<tr><td>Ground</td><td>Pin 25 - GND</td></tr>
<tr><td>Pixel Power</td><td>Not Connected to Pi</td></tr>
</table>
</center>
<p><b>RPIWS281X</b> - The RPIWS281X output can drive two independent strings of WS281x pixels directly off the Raspberry Pi's GPIO ports.  The data and ground lines attach directly to the Pi while power for the pixels is injected from another source.  <b>NOTE: When you enable the RPIWS281X output, the onboard audio on the Pi will be disabled since both audio and RPIWS281X need to use the same PWM output.</b></p>
<center>
<b>RPIWS281x Output Connections</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>WS281x Function</th><th>Raspberry Pi</th></tr>
<tr><td>Data String #1</td><td>Pin 12 - GPIO18</td></tr>
<tr><td>Data String #2</td><td>Pin 35 - GPIO19 (Only on 40-pin Pi's)</td></tr>
<tr><td>Ground</td><td>Pin 25 - GND</td></tr>
<tr><td>Pixel Power</td><td>Not Connected to Pi</td></tr>
</table>
</center>
<p><b>spixels</b> - The spixels output can drive 16 independent strings of WS2801, APA102, LPD6803, or LPD8806 pixels directly off the Raspberry Pi's GPIO ports by emulating SPI outputs in software.  The data and ground lines attach directly to the Pi while power for the pixels is injected from another source.</b></p>
<center>
<b>spixels Output Connections</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>WS2801/APA102/LPD Function</th><th>Raspberry Pi</th></tr>
<tr><td>Data String #1</td><td>Pin 12 - GPIO18</td></tr>
<tr><td>Data String #2</td><td>Pin 16 - GPIO23</td></tr>
<tr><td>Data String #3</td><td>Pin 15 - GPIO22</td></tr>
<tr><td>Data String #4</td><td>Pin 29 - GPIO5</td></tr>
<tr><td>Data String #5</td><td>Pin 32 - GPIO12</td></tr>
<tr><td>Data String #6</td><td>Pin 36 - GPIO16</td></tr>
<tr><td>Data String #7</td><td>Pin 35 - GPIO19</td></tr>
<tr><td>Data String #8</td><td>Pin 40 - GPIO21</td></tr>
<tr><td>Data String #9</td><td>Pin 7 - GPIO4</td></tr>
<tr><td>Data String #10</td><td>Pin 11 - GPIO17</td></tr>
<tr><td>Data String #11</td><td>Pin 18 - GPIO24</td></tr>
<tr><td>Data String #12</td><td>Pin 22 - GPIO25</td></tr>
<tr><td>Data String #13</td><td>Pin 31 - GPIO6</td></tr>
<tr><td>Data String #14</td><td>Pin 33 - GPIO13</td></tr>
<tr><td>Data String #15</td><td>Pin 37 - GPIO26</td></tr>
<tr><td>Data String #16</td><td>Pin 38 - GPIO20</td></tr>
<tr><td>Clock</td><td>Pin 13 - GPIO27</td></tr>
<tr><td>Ground</td><td>Pin 25 - GND</td></tr>
<tr><td>Pixel Power</td><td>Not Connected to Pi</td></tr>
</table>
</center>
<p><b>Triks-C</b> - The Triks-C output can drive up to four LEDTriks modules attached to Triks-C or Pix-C boards.  The start channel is specified along with the serial port the Triks-C is connected to and the panel layout.  Panels may be layed out in a horizontal row of 1-4 panels, a vertical column of 1-4 panels, or a 2x2 grid.  Panels must be numbered starting at the top left, going across then down.  In a 2x2 layout, the top row would be panels 1 and 2 and the bottom row would be panels 3 and 4 with panel 3 below panel 1 and panel 4 below panel 2.</p>
<p><b>GPIO</b> - The GPIO outut can drive a single GPIO pin high or low based on a channel's value being zero (low) or non-zero (high).  This may be used to drive output relays or trigger other attached devices.</p>
<center>
<b>GPIO Pins available on the Pi model B and B+</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>Name</th><th>BCM Pin #</th><th>Model B</th><th>Model B+</th></tr>
<tr><td>GPIO0</td><td>17</td><td>P1 - Pin 11</td><td>J8 - Pin 11</td></tr>
<tr><td>GPIO1</td><td>18</td><td>P1 - Pin 12</td><td>J8 - Pin 12</td></tr>
<tr><td>GPIO2</td><td>27</td><td>P1 - Pin 13</td><td>J8 - Pin 13</td></tr>
<tr><td>GPIO3</td><td>22</td><td>P1 - Pin 15</td><td>J8 - Pin 15</td></tr>
<tr><td>GPIO4</td><td>23</td><td>P1 - Pin 16</td><td>J8 - Pin 16</td></tr>
<tr><td>GPIO5</td><td>24</td><td>P1 - Pin 18</td><td>J8 - Pin 18</td></tr>
<tr><td>GPIO6</td><td>25</td><td>P1 - Pin 22</td><td>J8 - Pin 22</td></tr>
<tr><td>GPIO7</td><td>4</td><td>P1 - Pin 7</td><td>J8 - Pin 7</td></tr>
<tr><td>GPIO8</td><td>28</td><td>P5 - Pin 3</td><td>N/A</td></tr>
<tr><td>GPIO9</td><td>29</td><td>P5 - Pin 4</td><td>N/A</td></tr>
<tr><td>GPIO10</td><td>30</td><td>P5 - Pin 5</td><td>N/A</td></tr>
<tr><td>GPIO11</td><td>31</td><td>P5 - Pin 6</td><td>N/A</td></tr>
<tr><td>GPIO21</td><td>5</td><td>N/A</td><td>J8 - Pin 29</td></tr>
<tr><td>GPIO22</td><td>6</td><td>N/A</td><td>J8 - Pin 31</td></tr>
<tr><td>GPIO23</td><td>13</td><td>N/A</td><td>J8 - Pin 33</td></tr>
<tr><td>GPIO24</td><td>19</td><td>N/A</td><td>J8 - Pin 35</td></tr>
<tr><td>GPIO25</td><td>26</td><td>N/A</td><td>J8 - Pin 37</td></tr>
<tr><td>GPIO26</td><td>12</td><td>N/A</td><td>J8 - Pin 32</td></tr>
<tr><td>GPIO27</td><td>16</td><td>N/A</td><td>J8 - Pin 36</td></tr>
<tr><td>GPIO28</td><td>20</td><td>N/A</td><td>J8 - Pin 38</td></tr>
<tr><td>GPIO29</td><td>21</td><td>N/A</td><td>J8 - Pin 40</td></tr>
</table>
<br>
<b>GPIO Outputs on the PiFace</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>Name</th><th>Pin #</th></tr>
<tr><td>Output 1</td><td>200</td></tr>
<tr><td>Output 2</td><td>201</td></tr>
<tr><td>Output 3</td><td>202</td></tr>
<tr><td>Output 4</td><td>203</td></tr>
<tr><td>Output 5</td><td>204</td></tr>
<tr><td>Output 6</td><td>205</td></tr>
<tr><td>Output 7</td><td>206</td></tr>
<tr><td>Output 8</td><td>207</td></tr>
</table>
</center>
<p><b>GPIO-595</b> - The GPIO-595 output can drive up to 16 daisy-chained 74HC595 Shift Register IC's using a set of 3 GPIO Output pins. See the table below for connection information. Pick one set of outputs 'GPIO 17-18-27' or 'GPIO 22-23-24', do not connect the 74HC595 to both sets of GPIO Pins.</p>
<center>
<b>GPIO-595 Output Connections</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th rowspan='2'>Function</th><th rowspan='2'>74HC595<br>Pin #</th>
	<th colspan='2'>GPIO 17-18-27</th>
	<th colspan='2'>GPIO 22-23-24</th></tr>
<tr><th>BCM GPIO</th><th>Pin #</th><th>BCM GPIO</th><th>Pin #</th></tr>
<tr><td>Clock</td><td>11 - SRCLK</td><td>BCM 17</td><td>P1 - Pin 11</td><td>BCM 22</td><td>P1 - Pin 15</td></td></tr>
<tr><td>Data</td><td>14 - SER</td><td>BCM 18</td><td>P1 - Pin 12</td><td>BCM 23</td><td>P1 - Pin 16</td></tr>
<tr><td>Latch</td><td>12 - RCLK</td><td>BCM 27</td><td>P1 - Pin 13</td><td>BCM 24</td><td>P1 - Pin 18</td></tr>
</table>
</center>

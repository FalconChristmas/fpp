<h3>Channel Output Configuration</h3>

<p><b>E1.31</b> - The E1.31 output can drive up to 256 universes (131,072 channels) over the Pi's ethernet network interface at 50ms timing or 128 universes (65,536 channels) at 25ms timing.</p>
<p><b>Falcon Pixelnet/DMX (FPD)</b> - The FPD output can send 32,768 channels out 12 ports configured for the DMX or Pixelnet protocols.  This is currently limited to the first 32768 channels in a sequence.</p>
<p><b>DMX Open</b> - The DMX Open output can send DMX data out generic FTDI-based USB to RS485 dongles.  Other RS485 dongles may work if they support setting arbitrary bit rates.  Support should include the following dongles: Entec Open DMX, LOR, and D-Light along with generic FTDI-based USB to RS485 adapters.</p>
<p><b>DMX Pro</b> - The DMX Pro output can send DMX data out Entec-Pro compatible dongles.  This should include the following dongles: Entec-Pro, Lynx USB dongle w/ DMX firmware, DIYC RPM, DMXking.com, and DIYblinky.com.  If the dongle works using xLights DMX Pro output, it should work in the Falcon Player. </p>
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
<tr><td>+5V Power</td><td>Not Connected to Pi</td></tr>
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

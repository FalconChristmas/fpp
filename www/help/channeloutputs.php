<h3>Channel Output Configuration</h3>

<p><b>E1.31</b> - The E1.31 output can drive up to 128 universes (65,536 channels) over the Pi's ethernet network interface.</p>
<p><b>Falcon Pixelnet/DMX (FPD)</b> - The FPD output can send 32,768 channels out 12 ports.</p>
<p><b>DMX Open</b> - The DMX Open output can send DMX data out generic FTDI-based USB to RS485 dongles.</p>
<p><b>DMX Pro</b> - The DMX Pro output can send DMX data out Entec-Pro compatible dongles.</p>
<p><b>Pixelnet Open</b> - The Pixelnet Open output can send Pixelnet data (one 4096-channel universe) out generic FTDI-based USB to RS485 dongles.</p>
<p><b>Pixelnet Lynx</b> - The Pixelnet Lynx output can send Pixelnet data (one 4096-channel universe) out the DIYLightAnimation.com USB dongle with Pixelnet firmware.</p>
<p><b>Renard</b> - The Renard output can drive up to 4584 channels at the highest speeds using standard USB to serial dongles.</p>
<p><b>SPI-WS2801</b> - The SPI-WS2801 output can drive one string of WS2801 pixels directly off the Raspberry Pi's SPI port.  The data, clock, and ground lines attach directly to the Pi while power for the pixels is injected from another source.</p>
<center>
<b>SPI-WS2801 Output Connections</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>Function</th><th>Raspberry Pi</th></tr>
<tr><td>Clock</td><td>Pin 23 - SCLK</td></tr>
<tr><td>Data</td><td>Pin 19 - MOSI</td></tr>
<tr><td>Ground</td><td>Pin 25 - GND</td></tr>
<tr><td>+5V Power</td><td>Not Connected to Pi</td></tr>
</table>
</center>
<p><b>Triks-C</b> - The Triks-C output can drive up to four LEDTriks modules attached to Triks-C or Pix-C boards.  The start channel is specified along with the serial port the Triks-C is connected to and the panel layout.  Panels may be layed out in a horizontal row of 1-4 panels, a vertical column of 1-4 panels, or a 2x2 grid.  Panels must be numbered starting at the top left, going across then down.  In a 2x2 layout, the top row would be panels 1 and 2 and the bottom row would be panels 3 and 4 with panel 3 below panel 1 and panel 4 below panel 2.</p>
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

<h3>Channel Output Configuration</h3>

<p><b>E1.31</b> - The E1.31 output can drive up to 512 universes over the Pi v2 B's ethernet network interface at 50ms timing or 128 universes on the original A/B/B+.  BeagleBone output should be similar to the Pi v2 B.</p>

<p><b>Falcon Pixelnet/DMX (FPD)</b> - The FPD output can send 32,768 channels out 12 ports configured for the DMX or Pixelnet protocols.  This is currently limited to the first 32768 channels in a sequence.</p>

<p><b>DMX Open</b> - The DMX Open output can send DMX data out generic FTDI-based USB to RS485 dongles.  Other RS485 dongles may work if they support setting arbitrary bit rates.  Support should include the following dongles: Entec Open DMX, LOR, and D-Light along with generic FTDI-based USB to RS485 adapters.</p>

<p><b>DMX Pro</b> - The DMX Pro output can send DMX data out Entec-Pro compatible dongles.  This should include the following dongles: Entec-Pro, Lynx USB dongle w/ DMX firmware, DIYC RPM, DMXking.com, and DIYblinky.com.  If the dongle works using xLights DMX Pro output, it should work in the Falcon Player. </p>

<p><b>RGBMatrix</b> - The RGBMatrix output can drive up to 36 of the HUB75 style 32x16 RGB LED Panels.  These panels may be wired directly to the Pi's GPIO header or an adapter board may be used to handle the wiring.  The RGBMatrix output uses librgbmatrix from Henner Zeller's rpi-rgb-led-matrix git repository to drive HUB75 panels connected to a Raspberry Pi.  If you wish to make your own board or manually connect a panel, the wiring pinout is available at <a href='https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/wiring.md'>https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/wiring.md</a>.</p>

<p><b>Pixelnet Open</b> - The Pixelnet Open output can send Pixelnet data (one 4096-channel universe) out generic FTDI-based USB to RS485 dongles.</p>

<p><b>Pixelnet Lynx</b> - The Pixelnet Lynx output can send Pixelnet data (one 4096-channel universe) out the Lynx USB dongle w/ Pixelnet firmware.</p>

<p><b>Renard</b> - The Renard output can drive up to 4584 channels at the highest speeds using standard USB to serial dongles.</p>

<p><b>SPI-WS2801</b> - The SPI-WS2801 output can drive one string of WS2801 pixels directly off the Raspberry Pi's SPI port.  The data, clock, and ground lines attach directly to the Pi while power for the pixels is injected from another source.</p>
<p>
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
</p>

<p><b>RPIWS281X</b> - The RPIWS281X output can drive two independent strings of WS281x pixels directly off the Raspberry Pi's GPIO ports.  The data and ground lines attach directly to the Pi while power for the pixels is injected from another source.  <b>NOTE: When you enable the RPIWS281X output, the onboard audio on the Pi will be disabled since both audio and RPIWS281X need to use the same PWM output.</b></p>
<p>
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
</p>

<p><b>spixels</b> - The spixels output can drive 16 independent strings of WS2801, APA102, LPD6803, or LPD8806 pixels directly off the Raspberry Pi's GPIO ports by emulating SPI outputs in software.  The data and ground lines attach directly to the Pi while power for the pixels is injected from another source.</b></p>
<p>
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
</p>

<p><b>GPIO</b> - The GPIO outut can drive a single GPIO pin high or low based on a channel's value being zero (low) or non-zero (high).  This may be used to drive output relays or trigger other attached devices.</p>
<p>
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
</p>

<p><b>GPIO-595</b> - The GPIO-595 output can drive up to 16 daisy-chained 74HC595 Shift Register IC's using a set of 3 GPIO Output pins. See the table below for connection information. Pick one set of outputs 'GPIO 17-18-27' or 'GPIO 22-23-24', do not connect the 74HC595 to both sets of GPIO Pins.</p>
<p>
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
</p>

<p><b>DPIPixels</b> - The DPIPixels Channel Output on the Raspberry Pi can drive up to 24 strings of LED pixels connected directly to the GPIO pins (a voltage shifter might also be needed since the GPIO pins are only 3.3V).  The number of pixels is determined by the frame rate and protocol.  For example, at 20 FPS over 1,600 WS281X pixels can be chained from each GPIO pin (each WS281X pixel takes 30 usec), allowing a Raspberry Pi to run close to 40K WS281x pixels at 20 FPS.  <a id="more-dpi-info" href="#">Click here to show/hide more info</a>.  <b>NOTE: The DPIPixels-2 Channel Output is pin-compatible with the PiHat/RPIWS281X Channel Output but allows the onboard audio on the Pi to continue to function.</b><p>
<p class="more-dpi-info">DPI redirects video output from the GPU onto selected GPIO pins.  The RPi GPU serves as a high-speed 24-bit parallel port dongle, repeatedly refreshing the LED pixels from a GPU framebuffer and off-loading time-critical I/O from the RPi general processor cores.</p>
<p class="more-dpi-info">GPIO pins that are used for DPI are not available for other functions such as SPI, PMW, UART serial output, or sensors, but each GPIO can be configured individually for DPI so conflicts can be avoided - the tradeoff is fewer DPI Channel Outputs available for LED pixels.</p>
<p class="more-dpi-info">
<center class="more-dpi-info" NOTE="table won't hide without this">
<b>DPI Output Connections</b><br>
<table border="1" cellpadding="4" cellspacing="1" style="margin: 0 40px;">
<tr><th>RPi Pins</th><th>Function</th><th>Notes</th></tr>
<tr><td>GPIO 0 - 3</td><td>Normal function</td><td>DPI uses these pins for video CLK, EN, HSYNC and VSYNC signals, which are not useful for Channel Outputs.  These pins can be used for normal purposes.</td></tr>
<tr><td style="white-space: nowrap;">GPIO 4 - 27</td><td style="white-space: nowrap;">Channel Outputs</td><td>GPIO 4 - 27 can optionally be used for pixel data or control signals. Otherwise, these pins are available for normal purposes.</td></tr>
<tr><td>V+</td><td><em>Do not connect</em></td><td>The RPi power supply is not strong enough to run large numbers of LED pixels.  A separate power supply must be used.</td></tr>
<tr><td>Ground</td><td>Common</td><td>RPi ground must be connected to LED pixel ground to establish a common reference voltage.</td></tr>
</table>
</center>
</p>
<p class="more-dpi-info">DPI Channel Output in FPP currently only supports WS281X, but additional protocols might be added in future.  DPI could also be used for servo or DC dimmer control, but AC (phase angle) dimming is currently not possible directly from the GPIO pins because the RPi GPU is not synced to AC line voltage (an external controller with AC zero-cross detection would be needed).</p>

<style>
.back2top {
        display: block; /*required for right-justify*/
        margin-left: auto;
        margin-right: 0;
        z-index: 99;
        background: red;
        color: #fff;
        border-radius: 30px;
        padding: 15px;
        font-weight: bold;
        line-height: normal;
        border: none;
        opacity: 0.5;
        maybe-todo-content: "\f077";
        maybe-todo-font-family: FontAwesome;
    }
.more-dpi-info {
        display: none;
    }
#helpText {
    position: relative;
}
</style>

<button type="button" class="back2top">Back to top</button>

<script type="application/javascript">
$(document).ready(function() {
//.modal-body
//.modal-content
//.modal-dialog
//use this one => #dialog-help
  $(".back2top").click(() => $("#dialog-help").animate({scrollTop: 0}, "slow"));
  $(window).scroll(function(){
  console.log("help scrolled ", $(this).scrollTop());
      if ($(this).scrollTop() > 100) $('.back2top').fadeIn();
      else $('.back2top').fadeOut();
  });
  $("#more-dpi-info").click(() => $(".more-dpi-info").toggle({duration: 400}));
});
</script>

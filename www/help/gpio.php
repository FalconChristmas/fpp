<h3>GPIO Input Triggers</h3>

<p>GPIO Inputs allow the triggering of internal FPP Events via external input.  Each GPIO
Input is connected to a pin on the Pi's GPIO header or attached to an add-on I/O board
such as the PiFace.  GPIO Inputs allow two events to be attached.  These events are
triggered on the rising and falling of a GPIO pin.  In the Falcon Player, GPIO pins are
pulled high by default and can be connected to ground to pull them low via a switch.</p>

<p><b>Event Types</b></p>

<ul>
	<li><b>Rising</b> events are triggered when the GPIO pin goes high</li>
	<li><b>Falling</b> events are triggered when the GPIO pin goes low.</li>
</ul>

<p><b>Connection Types</b></p>
<ul>
	<li><b>Normally Open (NO)</b> - Normally Open connections are the most common type
		of connection because they close a circuit then activated.  Most push buttons
		are Normally Open and make contact internally then pressed.  To use a Normally
		Open button or device attached to a GPIO event, a Falling event is configured
		to trigger when the GPIO pin is pulled low by connecting it to ground.  A sample
		circuit diagram is provided below which pulls the pin high by default allowing
		the Normally Open button to pull the input low when activated.  To trigger an
		action when the button is released, attach a Rising event to the GPIO Input.<br>
		<br>
		<center><b>Sample GPIO Normally Open Button Configuration</b><br>
			<img src='http://<?= $_SERVER['SERVER_ADDR']; ?>/help/GPIO.png'></center></li>
	<li><b>Normally Closed (NC)</b> - Normally Closed connections complete a circuit
		in their default state and break the circuit upon activation.  One example
		device which may include a Normally Closed connection is a motion detection
		sensor.  To trigger an action when a Normally Closed device is activated,
		attach a Rising event to the GPIO Input.  To trigger when the device is
		deactivated, attach a Falling event to the GPIO Input.</li>
</ul>

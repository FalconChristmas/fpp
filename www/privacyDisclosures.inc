<?php
/*
 * What each privacy choice on the setup wizard actually discloses.
 *
 * Kept here rather than inline in the page for the same reason settings.json
 * and jurisdictions.json exist: this is the text FPP stands behind about where
 * a user's data goes, it has to be reviewable on its own, and more than one
 * consumer will want it (the wizard now, the Settings privacy tab next).
 *
 * Every factual claim below was checked against the code that implements it,
 * and several earlier drafts were wrong in ways worth not repeating:
 *
 *   - The publish interval is 7 days plus publish-on-change
 *     (STATS_PUBLISH_INTERVAL_DAYS in api/controllers/stats.php), NOT "every 10
 *     days and each restart", which is what it did before the change landed.
 *   - Statistics retention has no fixed deletion date.  Saying "400 days" would
 *     have been a promise nothing enforces.
 *   - The device ID is hardware-derived on Pi and BeagleBone, but scripts/get_uuid
 *     falls through to /etc/machine-id and then to a generated value, and the
 *     images ship machine-id as "uninitialized" so a freshly flashed card makes
 *     its own.  "Same after a reinstall" is therefore true of most players and
 *     not of all of them.
 *   - The crash-time log ring and fppd.log ship only at level 3
 *     (scripts/generate_crash_report), which is what keeps the level 1 promise
 *     honest -- and is also why the AI paragraph names level 3 specifically.
 *
 * Keys match the row ids in the privacy table.
 */

function privacyDisclosures()
{
    return array(

        'stats' => array(
            'title' => 'Share usage statistics',
            'body' => "
<p><b>Sent:</b> FPP version and branch, platform and OS, cape model, counts and total
sizes of your sequences, playlists and media, output and input configuration, installed
plugin names and versions, memory and storage sizes, CPU temperature, time zone, Wi-Fi
signal strength, and the values of settings that have a fixed list of choices.</p>

<p><b>A device ID</b> is included, taken from your hardware serial number or another
device-specific identifier, so repeat reports from this box count as one device rather
than many. On hardware that exposes a serial &mdash; Pi and BeagleBone both do &mdash; it
is the same ID after a reinstall or a new SD card. Where the hardware offers none, FPP
generates one and stores it, and a fresh install will produce a new one.</p>

<p><b>Other devices it can see on your network are described too</b> &mdash; not only other
FPP players, but any controller FPP discovers, such as Falcon, Genius, HinksPix,
SanDevices, AlphaPix, ESPixelStick or WLED. For each one: its device ID, type and model,
firmware or FPP version, platform and OS, cape model, player mode, the channel ranges it
drives and whether it has outputs or inputs enabled. Not every device reports every one of
those.</p>

<p>The device ID is the point of that list: it is what lets the project count a show once
rather than once per player, and it is how show and network sizes are worked out at all.
Without it, one ten-player show and ten separate single-player shows are the same thing to
whoever is reading the numbers.</p>

<p><b>Not sent for them:</b> their addresses, host names or descriptions, and never a cape
serial number. If your players are synced with someone else's &mdash; a club, or a street
&mdash; their devices are described the same way and counted with yours.</p>

<p><b>Not sent:</b> passwords, your e-mail address, your location, the IP addresses or
host names of your devices, your sequences or media, or anything you typed into a
free-text box. Your cape's serial number and licence key are not sent.</p>

<p>Sending anything reveals your <b>public IP address</b> to that server, as any internet
request does.</p>

<p><b>Recipients:</b> fppstats.falconchristmas.com (servers in the USA), and the FPP and
xLights developers, who use the reports to decide what to keep supporting. The charts on
that site are totals across all users; your own record is not published.</p>

<p><b>How often:</b> when this player's configuration changes, and at least once a week
so the project can see the device is still running.</p>

<p><b>Kept:</b> for as long as the project finds them useful. There is no fixed deletion
date, and older records are retained in archives.</p>

<p>Press <b>Preview data</b> to see exactly what this device would send. The preview is
generated on this device; nothing is sent.</p>",
        ),

        'crash' => array(
            'title' => 'Send crash reports',
            'body' => "
<p>A report is written to your <code>crashes</code> folder whether or not you send it, so
you can always open one and see for yourself. The ten most recent are kept on the box.</p>

<p><b>This tier sends:</b> the crash stack, the FPP version and build, your installed
plugin names and versions, and the playlist state at the moment of the crash &mdash;
section sizes and register values, which identify no one.</p>

<p><b>Not sent at this tier:</b> your host name, your devices' addresses, your settings,
your logs, or the statistics payload above &mdash; nothing about your setup. It is enough
to identify most crashes on its own. The file name carries the same device ID as the
statistics above, so reports from this box are linkable to each other.</p>

<p><b>Recipients:</b> a personal server run by one of the FPP developers (dankulp.com, in
the USA), used to diagnose the crash. Reports are read by the FPP and xLights developers.
Crash reports are not published.</p>

<p><b>Reports are also processed by an AI service.</b> Every morning the crash reports are
grouped automatically, and a summary of each one &mdash; the stack, the fault registers,
your platform and model, the device ID, and the recent log lines from the report &mdash;
is sent to Anthropic's Claude to help describe and diagnose them. When a developer
investigates a specific crash, the report itself may be opened the same way. At the
&ldquo;configuration and logs&rdquo; level those log lines can name devices on your
network, so that is the level at which this matters most.</p>",
        ),

        'crash-settings' => array(
            'title' => '&hellip; and include your settings',
            'body' => "
<p><b>Adds your FPP settings</b> to the report above.</p>

<p>This is usually what lets a developer reproduce the fault, because most crashes depend
on how the player is configured.</p>

<p>Passwords, Wi-Fi keys, your e-mail address, your latitude and longitude, your host name
and description, and your MQTT broker details are removed before it is sent.</p>

<p>It still describes your setup in detail: output types, channel counts, player mode,
which features you have enabled.</p>

<p>This tier includes everything above it.</p>",
        ),

        'crash-config' => array(
            'title' => '&hellip; and include configuration and logs',
            'body' => "
<p><b>Adds configuration files, FPP and system logs, and playlists.</b></p>

<p>The most useful tier for a developer, and the most revealing for you: <b>logs contain
the names and LAN addresses of devices on your network</b>, including devices that are not
FPP, and your playlists name your media files.</p>

<p>Your Wi-Fi configuration files are never copied at any tier, and credentials embedded
in URLs are stripped.</p>

<p>This is also the tier whose log lines reach the AI service described above.</p>

<p>Worth allowing if you are willing to help diagnose the harder crashes. This tier
includes everything above it.</p>",
        ),

        'email' => array(
            'title' => 'E-mail address',
            'body' => "
<p>Sent <b>with the crash reports you chose to send</b>, as its own field, and for nothing
else: a developer may use it to ask you for more detail about that crash. It is not
included in the settings file above and is not sent with statistics.</p>

<p>Never published, never shared, never added to any list. Leave it blank and your crash
reports carry no contact details.</p>",
        ),

        'logos' => array(
            'title' => 'Load cape logos from vendor website',
            'body' => "
<p><b>Every time you open the Cape Info page your browser contacts your cape maker's web
server, which tells that company your IP address, when you looked, and that you are using
their product.</b></p>

<p><b>Recipients:</b> whichever cape vendor made your hardware. They are separate
businesses from the FPP project, and what they keep is up to them.</p>

<p><b>Why you might allow it:</b> the volunteers who help people in the FPP support Zoom
room can see which cape is installed the moment you share your screen, instead of spending
the first few minutes of the session working out what hardware you have.</p>

<p>Denying it means no logo is shown, and anyone helping you has to ask what cape you have.
Nothing else changes.</p>",
        ),

        'serial' => array(
            'title' => '&hellip; and include your cape\'s serial number',
            'body' => "
<p><b>Your cape vendor sold you that cape and holds the order record, so the serial number
can identify you personally and tie you to your IP address.</b></p>

<p>The serial is added to the logo request above, so it can only be sent when that request
is allowed. Denying the logo denies this too.</p>

<p>It goes to the vendor only; it is not part of the usage statistics.</p>

<p>It is not needed for anything to work. Allow it only if you want your cape maker to be
able to recognise your device.</p>",
        ),

        'lookup' => array(
            'title' => 'Lookup Time Zone / Lookup Location',
            'body' => "
<p>Either button asks <b>ipapi.co</b>, a geolocation company in the USA, to guess your time
zone and location from a public internet address. If this player can reach the internet,
that is the player's address; if it cannot, your browser asks instead and it is your
browser's address. Nothing is sent unless you press a button; you can type the values in
instead.</p>

<p><b>Show On Map</b> opens Google Maps in a new tab with the coordinates shown here.</p>",
        ),
    );
}

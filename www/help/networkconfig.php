<h3>Network Configuration</h3>

<h4>Interface Settings</h4>
<p><b>Interface</b> — On a standard Raspberry Pi this will be <code>eth0</code> (wired Ethernet) and, if a WiFi adapter is present, <code>wlan0</code> (wireless). Other platforms may show differently named interfaces such as <code>eth1</code> or tethering interfaces. Select the interface you wish to configure from the dropdown. <i>Retained: the original help correctly notes that different hardware shows different interface names — keep it selected per interface.</i></p>

<p><b>Interface Mode</b> — <b>Static</b> is a fixed address you assign; it must be unique on the network — duplicate IPs cause conflicts and will confuse both devices. <b>DHCP</b> is an address automatically assigned by your router; as long as the router keeps the lease the address usually stays the same, but it can change after a router reset or lease expiry. In DHCP mode the next three fields (IP, Netmask, Gateway) are supplied by the router and cannot be set manually.</p>

<p><b>IP Address</b> — In Static mode enter the desired address as four decimal sections separated by periods (e.g. <code>192.168.0.26</code>). You do not need leading zeros, but all four sections are required. <i>Retained: original format and example kept.</i></p>

<p><b>Netmask</b> — In Static mode this is typically <code>255.255.255.0</code> for most home networks. If your network is unusual, use the netmask your network administrator provided. <i>Retained and expanded with default.</i></p>

<p><b>Gateway</b> — In Static mode this is typically one of two addresses on home routers: the router’s own address, usually ending in <code>.1</code> or <code>.254</code> (e.g. <code>192.168.0.1</code>). If unsure, check the network properties of a laptop or desktop on the same network. <i>Retained: original guidance on .1/.254 and where to look it up.</i></p>

<p><b>Update Interface</b> — Saves the current interface configuration to the on-device network storage (historically the flash drive). A <b>Restart Network</b> button will then appear; you can try restarting the network in place, but finishing your Interface <b>and</b> DNS settings first and then <b>Rebooting</b> from the main Status page is the most reliable, as some drivers only pick up changes after a reboot.</p>

<div class="callout callout-warning"><b>NOTE:</b> If you configure both eth0 and wlan0, they should not be on the same subnet. A common split is eth0 on a dedicated show LAN (e.g. <code>192.168.1.100</code>) and wlan0 on your home LAN via DHCP or a different Static subnet. Only <b>one</b> interface should have a Gateway. The interface that needs Internet access (for updates, time, or remote access) should be the one with the Gateway. <i>Retained: this dual-network warning is the most important original note and is kept verbatim in meaning.</i></div>

<h4>DNS Settings</h4>

<p><b>HostName</b> — A short, single-word name for this FPP (8 letters or fewer, alphanumeric plus hyphens, no symbols or spaces) and must be unique on your network. FPP uses it to discover peers and lets you reach the player as <code>http://hostname.local</code> in your browser. Common examples are <code>FPP1</code>, <code>FPP2</code>, <code>FPPM</code>, <code>FPPS1</code>, etc.</p>
<p><b>Save</b> — Writes the hostname change to the device’s persistent configuration.</p>

<p><b>DNS Server Mode</b> — <b>Manual</b> (enter your own) or <b>DHCP</b> (use the servers DHCP provides).</p>

<p><b>DNS Server 1</b> — In Manual mode, enter a DNS address in the same <code>###.###.###.###</code> format — most often your router’s IP, or a public server such as <code>8.8.8.8</code> (Google).</p>

<p><b>DNS Server 2</b> — Optional secondary in the same format, used if the first is unavailable.</p>

<p><b>Update DNS</b> — Writes the DNS/hostname settings. A <b>Restart DNS</b> button then appears to restart the DNS resolver without a full reboot.</p>

<h4>Extra topics shown on this page</h4>
<ul>
    <li><b>Tethering / Access Point</b> (when supported) — Turns this FPP into its own WiFi network for field configuration. It creates an SSID/passphrase and assigns addresses to clients; the tethered interface should not also have a separate Gateway beyond the tether subnet.</li>
    <li><b>Advanced Interface knobs</b> — Routing metrics, IP forwarding/NAT, and DHCP server settings appear under Advanced when the UI level permits. Leave them at defaults unless your network design requires them; they control packet forwarding between interfaces and whether this FPP hands out addresses to other devices.</li>
</ul>

<p><b>Tip:</b> After changing interfaces and DNS, apply <b>Update Interface</b> and <b>Update DNS</b> one at a time, then use the Status page <b>Reboot</b> for the cleanest transition. Verify new addresses in the header’s <b>IPs</b> display and by pinging the new hostname.</p>

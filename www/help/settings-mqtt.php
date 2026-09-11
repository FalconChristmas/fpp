<h3>MQTT</h3>
<p>Connects FPP to an MQTT broker so home automation, buttons or cloud services can observe and control the show. When a broker host is entered the remaining fields appear.</p>

<ul>
    <li><b>Broker Host</b> (text, pii) — Hostname or IP of the MQTT broker, e.g. <code>192.168.1.10</code> or <code>mqtt.local</code>. Controls visibility of all other MQTT fields.</li>
    <li><b>Broker Port</b> (number, 1–65535) — Broker’s TCP port. Default 1883 (plain), 8883 is typical for TLS. <i>Restarts FPPD to apply.</i></li>
    <li><b>Client ID</b> (text, pii) — Unique ID this FPP uses when connecting. Leave blank to auto-generate a random one. Set a fixed value only if your broker requires it.</li>
    <li><b>Topic Prefix</b> (text, pii) — String prepended to every topic FPP publishes, e.g. <code>myprefix/falcon/player/FPP/</code>. All publish and command topics sit under this prefix plus the hostname.</li>
    <li><b>Username</b> (text, pii) — Login for the broker. Leave blank for anonymous brokers. When set, the Password field is shown after it.</li>
    <li><b>Password</b> (password) — Password for the username above. Appears once Username is set.</li>
    <li><b>CA File</b> (text) — Path to the signer/CA certificate when using MQTT-SSL with a self-signed broker. Not needed for public or non-TLS brokers.</li>
    <li><b>Publish Playlist Frequency</b> (number, 0–3600) — How often to publish playlist state as JSON. 0 = on demand / change only, not periodic.</li>
    <li><b>Publish Port Status Frequency</b> (number, 0–3600) — How often to publish output port sensor data as JSON. 0 = disabled.</li>
    <li><b>Publish FPPD Status Frequency</b> (number, 0–3600) — How often to publish full fppd status JSON. 0 = disabled.</li>
    <li><b>Subscribe Topics</b> (text, pii) — Additional topics to subscribe to besides FPP’s own command topics. Use <code>#</code> for all topics, <code>smartthings/#</code> for a subtree, or an exact topic. Separate multiple with <code>;</code>. Received payloads appear via <code>api/fppd/mqtt/cache</code> and playlist branching.</li>
</ul>

<p><b>More detail:</b> For the full reference of topics FPP publishes (<code>ready</code>, <code>version</code>, <code>status</code>, <code>playlist/*</code>, <code>gpio/*</code>, <code>command/*</code>) and subscribes to (<code>/set/command/…</code>, <code>/set/playlist/…</code>, <code>/light/MODEL/cmd</code>), plus Home Assistant examples, see <a href='javascript:void(0)' onClick="helpPage='help/mqtt.php'; DisplayHelp();">Help → MQTT Integration</a>.</p>

<h3>VBAN Audio Streaming</h3>
<p>VBAN (VB-Audio Network) carries uncompressed PCM audio over UDP on your local
network. It is the protocol Voicemeeter, VBAN Talkie and the other VB-Audio tools
speak, which makes it the simplest way to get audio from a Windows or macOS
desktop into FPP — or to send FPP's audio back the other way.</p>

<p>Each stream you configure here becomes a node in FPP's PipeWire audio graph:</p>
<ul>
    <li>A <b>receive</b> stream appears as an audio source. Use it as a member on
        the <b>Input Mixing</b> page to route it to any output group, and pick it as
        the <b>WLED Sound Reactive Source</b> under Settings &rarr; Audio/Video to
        drive sound-reactive effects.</li>
    <li>A <b>send</b> stream appears as an audio sink. Add it to an
        <b>Audio Output Group</b> and whatever that group plays is transmitted to
        the destination. Nothing is sent until a group feeds it.</li>
</ul>

<h4>Before you start</h4>
<ul>
    <li>VBAN requires the <b>Advanced (Full PipeWire)</b> media backend. Set it
        under Settings &rarr; Audio/Video and reboot.</li>
    <li>PipeWire reads its module list only when it starts, so <b>Save &amp; Apply
        restarts the PipeWire stack and fppd</b> and audio stops for a few seconds.
        Apply VBAN changes when nothing is playing.</li>
</ul>

<h4>Stream settings</h4>
<ul>
    <li><b>Enable</b> (switch) — Turns the stream on or off. A disabled stream stays
        configured but no PipeWire node is created for it.</li>
    <li><b>Name</b> (text) — Your label for the stream. It also decides the PipeWire
        node name shown at the bottom of the card, so renaming a stream that is
        already used by an input or output group means re-selecting it there.</li>
    <li><b>Mode</b> (dropdown) — <b>Receive</b> takes audio from the network,
        <b>Send</b> transmits it, <b>Both</b> does each on the same port.</li>
    <li><b>VBAN Stream Name</b> (text, max 16 characters) — The name carried inside
        every VBAN packet. Voicemeeter's default is <code>Stream1</code>, and it must
        match at both ends. On a receiver you may leave it blank to accept whatever
        arrives; two receivers sharing one port must have different names so FPP can
        tell their audio apart.</li>
    <li><b>Listen Address</b> (text, receive only) — Which address to bind the
        receive socket to. <code>0.0.0.0</code> accepts from any sender on any
        interface and is almost always the right answer.</li>
    <li><b>Destination IP</b> (text, send only) — The address of the machine running
        the VBAN receiver, for example the PC running Voicemeeter.</li>
    <li><b>UDP Port</b> (number) — VBAN's default is <b>6980</b>. Several receivers
        may share a port when their stream names differ; two senders must not share
        the same destination address and port.</li>
    <li><b>Channels</b> (dropdown) — Channel count for the stream. It must match what
        the far end is sending or expecting.</li>
    <li><b>Sample Rate</b> (dropdown, send only) — Usually 48000 Hz. It must match
        what the receiving application expects.</li>
    <li><b>Sample Format</b> (dropdown, send only) — 16-bit PCM is the most widely
        supported choice and what Voicemeeter uses by default.</li>
    <li><b>Jitter Buffer</b> (number, receive only) — How much network timing
        variation to absorb, in milliseconds. <b>100</b> suits most links. Both
        extremes cause dropouts, for opposite reasons: too low and the bursty way
        senders such as Voicemeeter transmit will underrun it, while too high makes
        every out-of-order packet more costly (see below). When several receivers
        share a port, the largest value on that port applies to all of them.</li>
    <li><b>Network Interface</b> (dropdown) — Leave on <i>(Default)</i> unless this
        device has more than one network connection and the stream must use a
        specific one.</li>
</ul>

<h4>Status badges</h4>
<ul>
    <li><b>Live</b> — The PipeWire node exists and is part of the audio graph.</li>
    <li><b>Waiting</b> — The stream is configured and applied, but no node is present
        yet. For a receiver this is the normal appearance when nothing is being sent
        to FPP: the node is created the moment a packet with a matching stream name
        arrives.</li>
    <li><b>Not applied</b> — The stream has been saved but the PipeWire configuration
        has not been regenerated yet. Use <b>Save &amp; Apply</b>.</li>
    <li><b>Disabled</b> — The stream is switched off.</li>
</ul>

<h4>Sending audio from Voicemeeter to FPP</h4>
<ol>
    <li>In Voicemeeter, open the VBAN panel, enable an outgoing stream, set its
        destination to this FPP device's IP address, and note the stream name, port,
        sample rate and channel count.</li>
    <li>Here, add a stream in <b>Receive</b> mode with the same stream name and port.</li>
    <li>Click <b>Save &amp; Apply</b> and wait for PipeWire to restart.</li>
    <li>The badge turns <b>Live</b> once audio arrives. Then either add it as a member
        on the <b>Input Mixing</b> page to hear it, or select it as the
        <b>WLED Sound Reactive Source</b>.</li>
</ol>

<h4>If a receive stream never goes live</h4>
<ul>
    <li>Confirm the sender is pointed at this device's IP address and that the port
        matches on both sides.</li>
    <li>Confirm the VBAN stream name matches exactly — it is case sensitive.</li>
    <li>Check that no firewall between the two machines is blocking the UDP port.</li>
    <li>If audio arrives but breaks up, see the next section before reaching for
        the Jitter Buffer — raising it is as likely to hurt as help.</li>
</ul>

<h4>Regular short dropouts</h4>
<p>If the audio drops out briefly at fairly regular intervals — every couple of
seconds, say — time one of the gaps. If each gap is about the same length as the
<b>Jitter Buffer</b> setting, the cause is <b>packet reordering on the network</b>,
not FPP and not the sender.</p>
<p>VBAN numbers every packet. If a packet arrives after one that was sent later,
the receiver sees the count go backwards, resynchronises, and discards the whole
buffer — so you hear exactly one buffer's worth of silence. The rate of those
incidents is set by the network; only the length of each is yours to control.</p>
<ul>
    <li><b>Reduce the damage:</b> lower the <b>Jitter Buffer</b> (try 60). The
        dropouts happen just as often but each is far shorter and much less
        noticeable. Below about 40&nbsp;ms you start trading them for underruns
        instead.</li>
    <li><b>Remove the cause</b>, on the sending machine or the network between:
        <ul>
            <li>Prefer a wired connection; Wi-Fi reorders readily.</li>
            <li>On the sender's network adapter, turn down interrupt moderation
                and any multi-queue or offload feature that batches transmits.</li>
            <li>Try a different switch or port, and avoid powerline and
                MoCA links, which reorder under load.</li>
            <li>In Voicemeeter, a smaller VBAN packet/quality setting sends
                less bursty traffic and gives reordering less opportunity.</li>
        </ul></li>
</ul>
<p>Sample rate is not a factor here — matching the stream to the sound card's rate
does not affect reordering.</p>

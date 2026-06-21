# ColorLight Output Too Slow
Frames are repeatedly taking more than 20ms to send to the ColorLight interface, which limits frame rate and can stutter playback.

1. Use a dedicated wired NIC for the ColorLight link; do not share it with other traffic.
2. Confirm the interface negotiates 1000Mbps (see the ColorLight Link/Speed warnings).
3. Reduce per-frame channel count or output frame rate if the link cannot keep up.

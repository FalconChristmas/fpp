# ColorLight Output Too Slow
The ColorLight interface is not taking frames as fast as FPP produces them, so whole frames are being dropped. The receiver keeps showing the last complete frame, so playback stutters rather than tearing, and the other outputs keep their timing. The warning clears by itself about 30 seconds after frames stop being dropped.

1. Use a dedicated wired NIC for the ColorLight link; do not share it with other traffic.
2. Confirm the interface negotiates 1000Mbps (see the ColorLight Link/Speed warnings).
3. A USB network adapter on a single-core board (a BeagleBone) can run out of CPU before it runs out of bandwidth. Reduce per-frame channel count or output frame rate if the link cannot keep up.

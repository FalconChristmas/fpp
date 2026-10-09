# Controller Needs More Than Its Pacing Rate
FPP limits how fast it sends data to each unicast controller (the **Pacing** rate), so that a
gigabit player doesn't overrun the switch buffers in front of a 100 Mbps controller. The
controller named in the warning needs more data per second than that rate allows. FPP can't get
each frame to it in time, so frames arrive late or cut short and its lights stutter or flicker.
The warning gives the rate the controller needs and the rate it is paced at.

To fix it, either let the controller receive faster or send it less:

1. **If the controller has a gigabit network port** (another FPP, or a gigabit-capable pixel
   controller), raise its pacing. On the [Channel Outputs](../../channeloutputs.php#tab-e131)
   page, set that controller's **Pacing** override to a rate above what the warning says it
   needs, such as 200, 450 or 900 Mbps. The **Pacing** column and the global **Pacing** setting
   are only shown when **User Interface Level** is **Advanced** or higher. Set it on the
   [System](../../settings.php#settings-system) settings page.
2. **If the controller has a 100 Mbps port**, it cannot take much more than 90 Mbps, so reduce
   what is sent to it:
   - Lower the sequence frame rate, for example from 40 fps to 20 fps. That halves the data.
   - Use **DDP** instead of E1.31 or ArtNet if the controller supports it. E1.31 adds about a
     third on top of the pixel data in packet headers, and DDP adds a few percent.
   - Move some of its pixels to another controller.
3. **Don't just set Pacing to Disabled.** FPP then sends at full line rate and the switch drops
   whatever the 100 Mbps port can't absorb. Those losses are silent, and the flicker is usually
   worse.

Next to **Outputs Count** on the same page, FPP shows the highest frame rate the current pacing
allows for the slowest controller. Use it to check a change before you play the sequence again.

The warning clears by itself on the first check (about once a minute) where the controller keeps
up, or about five minutes after output stops.

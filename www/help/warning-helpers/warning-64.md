# Out of memory allocating pixel overlay model buffers

FPP could not allocate the memory a pixel overlay model needs. Each model holds a channel data buffer, and models that are drawn into also hold an overlay buffer sized `width x height x bytes-per-pixel`. FPP tries shared memory first so external programs can read the model, falls back to private memory, and only raises this warning when both of those and a plain allocation have all failed.

A model that cannot get its channel data is dropped and will not appear in the models list. A model that cannot get its overlay buffer stays in the list but will not display anything drawn into it.

Common causes:

- **Too much overlay memory configured for the board.** An overlay buffer is `width x height x 3` (or `x 4` with white), so a few large matrix models add up quickly on a board with limited RAM.
- **Shared memory exhausted.** `/dev/shm` is sized from total RAM and is shared with everything else using it.
- **Another process on the machine is consuming the memory**, for example a large sequence being uploaded or converted at the same time.

This warning stays up until `fppd` restarts — nothing retries the allocation.

1. Open the [Pixel Overlay Models](../../pixeloverlaymodels.php) page and check the sizes of the configured models, especially any matrix models.
2. Remove or shrink models you are not using, then restart `fppd`.
3. Check `fppd.log` for the model name and the byte count in the `Out of memory allocating` line.
4. Run `free -h` and `df -h /dev/shm` on the device to see whether memory is genuinely exhausted or something else is holding it.

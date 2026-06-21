# LOR Output Configuration Error
A Light-O-Rama (LOR) output unit is misconfigured, so data for that unit is not being sent. The message names the unit and problem:

- **Invalid start pixel** — the unit's start pixel is out of range.
- **More than 510 channels (170 pixels) per unit** — a single LOR unit cannot exceed 170 RGB pixels.
- **Channel range exceeds the configured channel count** — the unit maps beyond the output's channels.
- **Invalid unit ID** — LOR unit IDs must be 1–240.

1. Open [Channel Outputs](../../channeloutputs.php) and edit the LOR output's unit settings (unit ID, start pixel, pixel count, channel range).
2. Split units larger than 170 pixels across multiple LOR units.
3. Make sure the output's total channel count covers all configured units.

This warning clears automatically a short time after the configuration is corrected.

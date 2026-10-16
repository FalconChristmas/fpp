# Invalid Schedule Entry
A schedule entry is malformed or incomplete, so it cannot run as expected. The message names the problem:

- **Missing or invalid startTime / endTime** — the entry has no usable start or end time.
- **Invalid Schedule Entry (… elements)** — the saved entry is missing fields (often a leftover from an older format).
- **Schedule entry N is invalid and was skipped** — a field in the saved entry has the wrong type (for example a number saved as text), so FPP skipped that entry instead of loading it.
- **Unknown holiday / lunar calendar type** — the entry refers to a holiday or calendar type FPP does not recognize, so its date cannot be calculated.

1. Open the [Scheduler](../../scheduler.php) and review the listed schedule entry.
2. Re-set the start and end times (and the date/holiday options if used), then save the schedule.
3. If an entry looks corrupt, delete it and re-create it.
4. Check `fppd.log` for the specific entry that triggered the error.

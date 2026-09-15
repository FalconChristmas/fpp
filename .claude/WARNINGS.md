# fppd Warnings (`src/Warnings.h/cpp`)

Warnings are the banner FPP shows across the top of every web page. They are the
main way fppd tells a user something is wrong, and they are unusually easy to get
wrong in one specific direction: **a warning that is raised and never retracted
stays on screen until fppd restarts.**

## Why a stale warning is worse than no warning

There is no expiry unless you ask for one, and no way for a user to dismiss one:

- `AddWarning(...)` has **no timeout**. The warning lives until something calls
  `RemoveWarning()` with a matching id *and* message.
- `RemoveAllWarnings()` is called from exactly one place — the crash handler in
  [src/fppd.cpp](../src/fppd.cpp). It is not a periodic reset.
- The web UI has no dismiss button. `www/js/fpp.js` renders whatever
  `warnings.json` contains.

So a condition that recovers on its own — a network blip, a card replugged, a
clock that finishes converging, a config the user has since fixed and re-applied
— leaves a banner behind that contradicts the running system. Users act on that:
they chase a problem that is already solved, or they learn to ignore the banner,
which costs them the next real one.

## Required: every warning needs both halves

**When you add a `WarningHolder::AddWarning()` call, decide in the same change
how it comes down.** There are three legitimate answers, and you must pick one
consciously:

1. **`AddWarningTimeout(seconds, id, msg)`** — the warning ages out on its own.
   Best for anything polled: re-add it on each poll where the condition still
   holds and give it a timeout comfortably longer than the poll interval. The
   condition stops being true, the banner disappears, and there is no retraction
   logic to forget. Note `AddWarningTimeout` only ever pushes an existing
   warning's timeout *later*, never earlier, so re-adding is safe.
2. **A matching `RemoveWarning(id, msg)` on the recovery path** — for conditions
   with a definite "it is fixed now" moment (a reconnect, a successful re-apply,
   a device reappearing). Follow the rules below or it silently will not match.
3. **Deliberately permanent** — the condition genuinely cannot change without a
   restart (a config parse error on a channel output, "FPPD has crashed"). This
   is a real and common answer in this codebase. Say so in a comment so the next
   reader knows it was a decision rather than an omission.

If you cannot name which of the three applies, the warning is not finished.

## `RemoveWarning()` matches on the exact message text

This is the trap that has bitten this codebase repeatedly.
`WarningHolder::RemoveWarning(id, w, plugin)` walks the list comparing
`id() == id && message() == w && plugin() == plugin`. An id match is **not**
enough. A message assembled slightly differently at the remove site does nothing
at all — no error, no log, the banner just stays.

Two patterns that work:

**Share the string.** Put it in a named constant both sides use:

```cpp
// src/mediaoutput/AES67Manager.h
constexpr int WARNING_ID_PIPELINE = 44;
constexpr const char* WARNING_SEND_FAILED = "AES67: audio send stream failed to start";
```

**Store what you actually added**, when the message interpolates a variable:

```cpp
// src/OutputMonitor.cpp -- clearEFuseWarning()
WarningHolder::RemoveWarning(16, port->receivers[rec].warning);
port->receivers[rec].warning.clear();
```

`BBShiftStringOutput::clearBudgetWarning()` in
[src/non-gpl/BBShiftString/BBShiftString.cpp](../src/non-gpl/BBShiftString/BBShiftString.cpp)
does the same thing. Both are good models.

**Never** retype the message literal at the remove site. A real bug from this:
`CheckAudioOutputCardPresence()` in
[src/mediaoutput/mediaoutput.cpp](../src/mediaoutput/mediaoutput.cpp) adds
`"Audio output card '" + id + "' is not present - …"` but removes
`"Audio output card is not present"` — a string that is never added, so the
"card came back" path clears nothing.

## Raising and retracting from more than one place

When a warning's state is tracked by a flag (`warningActive`, `m_ptpNoLockWarned`,
`piPowerWarningAdded`), the flag and the banner must move together. Wrap both in
a small pair of `Raise…()` / `Clear…()` helpers and call nothing else — a flag
that says "warning is up" while the banner is down, or the reverse, means the
recovery path either never fires or fires against nothing. See
`RaisePtpNoLockWarning()` / `ClearPtpNoLockWarning()` in
[src/mediaoutput/AES67Manager.cpp](../src/mediaoutput/AES67Manager.cpp).

## Watchdogs must review, not just detect

A warning raised at startup about a transient condition needs something that
re-checks it later. If a module already has a watchdog or poll loop, that is
where the review belongs. Be careful that the *detector* actually covers the
recovery: the AES67 PTP step detector compares consecutive 30s samples, so a
clock that slews into lock over ten minutes never trips it — reviewing the
warning needed a separate baseline that does not move.

## Warning ids

The `id` groups a warning for the UI and for the per-warning help text in
`www/help/warning-helpers/warning-<id>.md`. Ids are hand-assigned integers with
no central registry, which has already produced one collision: **44 is used by
both `AES67::WARNING_ID_PIPELINE` and `OpusRTPManager`**. Before picking a new
id, grep for it:

```bash
grep -rn "AddWarning(<id>," src/
ls www/help/warning-helpers/
```

Prefer a named constant near the warning's strings over a bare literal, and add
a `warning-<id>.md` helper for anything a user might need to act on.

## Checklist for a new warning

1. Pick a fresh id and give it a name, not a literal.
2. Choose timeout / explicit removal / deliberately permanent, and say which.
3. If it is removable, share the message string or store the one you added.
4. If a flag tracks it, put the flag and the banner in one pair of helpers.
5. If the condition is transient, make sure something re-checks it.
6. Add `www/help/warning-helpers/warning-<id>.md` if the user has to do
   something about it.

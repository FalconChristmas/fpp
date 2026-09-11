# Legitimate Interests Assessment — crash reports

**Scope:** crash reports sent at `ShareCrashData` level 1 ("send stack traces only"), including
the level FPP falls back to before a user has answered.
**Basis relied on:** GDPR Article 6(1)(f), legitimate interests.
**Not covered by this assessment:** levels 2 and 3, and the usage statistics. Those rest on
consent, for the reasons in the balancing test below.

This document exists because relying on legitimate interests requires the assessment to be
written down and kept. It records the position as of `privacyConsentVersion` 1. Revisit it when
what a level 1 report contains changes, when a recipient changes, or when the retention position
changes.

---

## 1. Purpose test — is there a legitimate interest?

FPP is a long-running daemon that drives lighting hardware, usually unattended, often on a
network with no route to the internet, across a wide spread of boards, capes and third-party
plugins. Crashes are frequently specific to a combination the developers do not own and cannot
reproduce.

The interest is **fixing defects in software people are relying on**. It is the FPP project's own
interest, and it is also the users' — a crash during a show is the failure mode that matters most
to them. Recital 47 contemplates precisely this kind of interest, and it is hard to argue a
volunteer project keeping its own software working is not legitimate.

The interest is not commercial. Nothing in a crash report is sold, published, or used for
advertising, profiling, or any decision about an individual.

## 2. Necessity test — is the processing necessary for it?

**Yes, and the level matters.** A level 1 report contains the crash stack, the fault registers,
the FPP version and build, the installed plugin names and versions, and the playlist state at the
moment of the crash. That is the minimum that identifies *which* defect occurred. Without the
stack there is nothing to group, rank or diagnose.

Alternatives were considered and are worse or insufficient:

- **Asking users to report crashes manually.** Tried in practice; the overwhelming majority of
  crashes are never reported. A report written to the box and never sent identifies nothing to
  the people who can fix it.
- **Aggregate counts only.** A count of crashes without a stack cannot be acted on.
- **Waiting for consent before sending anything.** A crash on first boot, before anyone has
  answered, is the one nobody can reproduce later — it is often exactly the configuration or
  hardware combination at fault.

Necessity is bounded by minimisation, which is what keeps this at level 1 rather than higher:
levels 2 and 3 add settings, configuration files and logs. Those are genuinely more useful for
diagnosis, and they are also where personal data lives, which is why they are **not** covered
here and are gated on consent instead.

## 3. Balancing test — do the individual's interests override it?

### What is actually sent at level 1

- The crash stack, fault registers, and the module the fault occurred in.
- FPP version, build commit, branch, platform and board model.
- Installed plugin names and versions.
- Playlist state at the moment of the crash — section sizes and positions, not names.
- The device ID, which is in the report's filename.
- The contact e-mail address, only if the user typed one in.

### What is deliberately not sent at level 1

Verified against `scripts/generate_crash_report`, which gates each tier explicitly:

- The settings file — so no host name, no coordinates, no Wi-Fi configuration, no MQTT broker,
  no passwords, and none of the identifiers marked `pii` in `settings.json`.
- The logs, including the crash-time ring buffer. FPP's logs carry host names and the LAN
  addresses of discovered devices, which is why they ship only at level 3.
- Configuration files, playlists by name, and the statistics payload.
- `fpp-info.json`, which carries the host name and the box's addresses.

### Reasonable expectations

Users of a self-hosted lighting controller expect the software to try to fix its own crashes.
The setting has been visible on the privacy tab since 2021 and in the setup wizard since 2022, so
this is not a facility introduced quietly. The setup wizard now states, before any answer is
given, what each tier sends and who receives it.

### Impact on the individual

Low at this tier. Nothing at level 1 describes the person, their household, their network or
their show. The residual points, stated plainly rather than minimised:

- **The device ID is stable and is in the file name**, so repeat reports from one box are
  linkable to each other and to that box's statistics records. This is deliberate — it is what
  makes "one site crash-looping" distinguishable from "a widespread bug" — but it is a persistent
  identifier and is treated as one.
- **Sending anything discloses the public IP address** to the receiving server, as any internet
  request does.
- **Reports are processed by an AI service.** Every morning they are grouped and a derived
  summary — stack, registers, platform, model, device ID, and the recent log lines the report
  contains — is sent to Anthropic's Claude to help describe and diagnose them, and a developer
  investigating a specific crash may open the report the same way. At level 1 those log lines are
  absent; this matters most at level 3, and is disclosed there.
- **Retention is not bounded by a policy.** Reports are kept while they are useful and older ones
  are archived. This is the weakest point of the assessment and is honestly the one to fix: Article
  5(1)(e) wants storage limitation, and "until we get around to it" is not a limitation. Until a
  retention rule exists, the notice says so rather than claiming otherwise.

### Safeguards

- Tiering, so the basis and the content match: legitimate interests covers only the tier that
  carries no personal data.
- A report is always written locally, whether or not it is sent, so the user can see exactly what
  a report contains. The ten most recent are kept on the box.
- Redaction is driven by declarations at the data (`"pii": true`, `"type": "password"` in
  `settings.json`; `"gatherStats": true` in `interface-settings.json`) rather than by lists
  maintained in each consumer, so a new sensitive setting is withheld by default rather than
  leaked until somebody remembers it.
- Credentials embedded in URLs are stripped, and Wi-Fi configuration files are never copied at
  any tier.

### Opt-out

Article 21(1) gives an absolute right to object. FPP's implementation is the setting itself:

- `ShareCrashData = 0` keeps reports local and sends nothing.
- `ShareCrashData = -1` does not write one at all.

The setting is presented during setup — where, under a regime requiring prior opt-in, the tiers
above 1 start unanswered and cannot be skipped — and is changeable at any time under
Content Setup → Privacy. Choosing to opt out requires no reason and takes effect immediately.

## 4. Outcome

Legitimate interests is an appropriate basis for **level 1 only**. The interest is real, the
processing is necessary to it, and at that tier the impact on the individual is low and bounded
by what the code actually sends.

It is **not** an appropriate basis for levels 2 and 3, which carry settings, configuration and
logs, and those remain on consent. It is not an appropriate basis for the usage statistics.

**Open item:** a retention policy for stored crash reports. Until one exists, the storage
limitation principle is not satisfied and the disclosure text says so.

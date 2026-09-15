# In-App Help Pages (`www/help/`)

Pressing **F1** on any FPP web page opens a modal that loads a help file from
`www/help/`. These files are hand-written prose describing the controls on the
page they document, so **they go stale silently** — nothing fails to build, no
test breaks, and the user just reads a description of a field that no longer
exists.

## Keeping help in sync — required

**When you add, remove, rename, or change the meaning of a user-facing control
on a page under `www/`, update that page's help file in the same change.**

This applies to form fields, buttons, tabs, table columns, settings entries in
`www/settings.json`, and anything else a user sees and interacts with. It is
not optional cleanup for later: the help file is part of the page.

Checklist for a `www/` change:

1. Work out which help file covers the page (see the mapping below).
2. If a help file exists, edit the matching section — add the new control,
   drop the removed one, correct wording when behaviour changes.
3. If no help file exists, you are not required to author a whole new one, but
   say so in your summary so the maintainer can decide.
4. Keep the help file's existing voice: user-facing, second person, describing
   *what the control does*, not how it is implemented.
5. Help files are PHP and follow `.claude/FRONTEND-GUIDELINES.md` like any
   other markup in `www/`.

## How a page resolves its help file

`DisplayHelp()` in [www/js/fpp.js](../www/js/fpp.js) loads the value of the
global `helpPage` variable into the modal. `config.php` sets it by default to
`help/<current page filename>` — so `www/backup.php` gets `www/help/backup.php`.

`www/settings.php` is the exception. Because every settings tab is one page,
`DisplayHelp()` reads the active tab id (`settings-<name>-tab`) and loads
`help/settings-<name>.php` instead. So the settings tabs map like this:

| Settings tab id             | Help file                        |
| --------------------------- | -------------------------------- |
| `settings-playback-tab`     | `help/settings-playback.php`     |
| `settings-av-tab`           | `help/settings-av.php`           |
| `settings-localization-tab` | `help/settings-localization.php` |
| `settings-ui-tab`           | `help/settings-ui.php`           |
| `settings-email-tab`        | `help/settings-email.php`        |
| `settings-mqtt-tab`         | `help/settings-mqtt.php`         |
| `settings-privacy-tab`      | `help/settings-privacy.php`      |
| `settings-output-tab`       | `help/settings-output.php`       |
| `settings-logs-tab`         | `help/settings-logs.php`         |
| `settings-services-tab`     | `help/settings-services.php`     |
| `settings-storage-tab`      | `help/settings-storage.php`      |
| `settings-system-tab`       | `help/settings-system.php`       |
| `settings-developer-tab`    | `help/settings-developer.php`    |

**If you add a settings tab to `www/settings.php`, add the matching
`help/settings-<name>.php`** — otherwise F1 on that tab shows only the generic
"No help file exists for this page yet" fallback.

Other conventions in this directory:

- A help file included from the settings modal must reset `helpPage` back to
  `"help/settings.php"` in a `<script>` block, because `config.php` overwrites
  it (see [www/help/settings-mqtt.php](../www/help/settings-mqtt.php)).
- Topic help files that are not tied to one page (e.g.
  [www/help/mqtt.php](../www/help/mqtt.php)) set `helpPage` to themselves so
  in-place navigation between help screens works.
- New standalone topic pages should also be listed in
  [www/help.php](../www/help.php), which is the browsable help index.
- `help/warning-helpers/warning-*.md` are a separate mechanism: markdown shown
  for specific fppd warnings, keyed by warning id.

## Settings metadata is a second source of truth

Most settings controls are declared in `www/settings.json` (and network ones in
`www/interface-settings.json`) with a `description` and `tip`. Those are the
inline hints; the `help/` file is the longer narrative. When you change a
setting, check whether both need updating — they usually do.

## If help page is missing
If the help page is missing and you have just made a change - generate a new help page populated with key info for that page.  If its a tabbed page ensure a help page is created for each tab

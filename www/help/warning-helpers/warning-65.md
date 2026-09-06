# FPPD restart limit reached

`systemd` has blocked `fppd` restarts after 5 failures within 200 seconds (`StartLimitBurst` / `StartLimitIntervalSec` in `fppd.service`). The daemon will not start until the interval passes.

**What to do:**

* Please wait the time shown in the warning (e.g. `1m 27s`) before clicking **Start FPPD** — `systemctl start fppd` will fail with `Start request repeated too quickly` until the 200-second window has elapsed.
* If you need it now, SSH in and run:

```bash
sudo systemctl reset-failed fppd && sudo systemctl start fppd
```

No reboot is needed. This is the same `Start request repeated too quickly` you see in `journalctl -u fppd` and `systemctl status fppd`.

On Docker or macOS `systemd` is not present, so this warning never appears.

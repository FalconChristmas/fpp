# Devcontainer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create `.devcontainer/devcontainer.json` so VS Code can open FPP in a fully-configured container with C++ IntelliSense, GDB debugging, and the full FPP stack running.

**Architecture:** A single `devcontainer.json` references the existing `Docker/Dockerfile`, mounts the repo at `/opt/fpp`, and runs FPP services via `postStartCommand`. No existing files are modified.

**Tech Stack:** VS Code Dev Containers, Docker, debian:trixie, GDB, Apache2, PHP-FPM 8.4

---

### Task 1: Create `.devcontainer/devcontainer.json`

**Files:**
- Create: `.devcontainer/devcontainer.json`

- [ ] **Step 1: Create the `.devcontainer` directory and `devcontainer.json`**

Create `.devcontainer/devcontainer.json` with the following exact content:

```json
{
    "name": "FPP Development",
    "build": {
        "dockerfile": "../Docker/Dockerfile",
        "context": "..",
        "args": {
            "EXTRA_INSTALL_FLAG": "--skip-clone",
            "FPPBRANCH": "master"
        }
    },
    "workspaceFolder": "/opt/fpp",
    "workspaceMount": "source=${localWorkspaceFolder},target=/opt/fpp,type=bind,consistency=cached",
    "remoteUser": "root",
    "postStartCommand": "/opt/fpp/src/fppinit start && /opt/fpp/scripts/fppd_start && mkdir -p /run/php && /usr/sbin/php-fpm8.4 --fpm-config /etc/php/8.4/fpm/php-fpm.conf && /usr/sbin/apache2ctl start",
    "customizations": {
        "vscode": {
            "extensions": [
                "ms-vscode.cpptools",
                "ms-vscode.cpptools-extension-pack",
                "xaver.clang-format"
            ]
        }
    },
    "forwardPorts": [
        "8080:80",
        32322
    ],
    "portsAttributes": {
        "8080:80": {
            "label": "FPP Web UI",
            "onAutoForward": "notify"
        },
        "32322": {
            "label": "FPPD/HTTP",
            "onAutoForward": "silent"
        }
    },
    "otherPortsAttributes": {
        "onAutoForward": "silent"
    }
}
```

Notes on key choices:
- `EXTRA_INSTALL_FLAG: --skip-clone` tells `FPP_Install.sh` to use the files already copied by `COPY ./ /opt/fpp/` rather than cloning from git.
- `workspaceMount` with `consistency=cached` mounts the host repo over `/opt/fpp`, replacing the Dockerfile's COPY'd files with your live source tree.
- `postStartCommand` starts all FPP services in daemon mode (none block). `fppinit start` sets up runtime dirs and permissions. `fppd_start` launches fppd. `php-fpm8.4` without `--nodaemonize` starts as a daemon. `apache2ctl start` starts Apache in background.
- Only TCP ports are forwarded (VS Code devcontainer port forwarding is TCP-only). UDP ports (DDP 4048, E1.31 5568, Multisync 32320) are accessible within the container's Docker network but not forwarded to the host.
- The existing `.vscode/launch.json` configs use `/opt/fpp/src/fppd` and `cppdbg`/`by-gdb` types which work as-is once `ms-vscode.cpptools` is installed.

- [ ] **Step 2: Verify the file was created correctly**

Run:
```bash
cat .devcontainer/devcontainer.json
```

Expected: the JSON content above printed to stdout with no errors.

- [ ] **Step 3: Validate JSON syntax**

Run:
```bash
python3 -c "import json, sys; json.load(open('.devcontainer/devcontainer.json')); print('JSON valid')"
```

Expected output: `JSON valid`

- [ ] **Step 4: Commit**

```bash
git add .devcontainer/devcontainer.json
git commit -m "feat: add devcontainer configuration"
```

---

### Task 2: Smoke-test the devcontainer build

This task validates the Dockerfile builds correctly with the devcontainer args before opening VS Code.

**Files:** (none created or modified)

- [ ] **Step 1: Build the image with devcontainer args to confirm no errors**

From the repo root, run:
```bash
docker build \
  -f Docker/Dockerfile \
  --build-arg EXTRA_INSTALL_FLAG=--skip-clone \
  --build-arg FPPBRANCH=master \
  -t fpp-devcontainer-test \
  .
```

Expected: build completes without error. This will take several minutes on first run (large package install). Subsequent runs use the Docker layer cache.

- [ ] **Step 2: Verify the image has GDB and expected tools**

Run:
```bash
docker run --rm fpp-devcontainer-test which gdb && \
docker run --rm fpp-devcontainer-test which apache2ctl && \
docker run --rm fpp-devcontainer-test which php8.4
```

Expected output:
```
/usr/bin/gdb
/usr/sbin/apache2ctl
/usr/bin/php8.4
```

- [ ] **Step 3: Clean up test image**

```bash
docker rmi fpp-devcontainer-test
```

Expected: image removed successfully.

- [ ] **Step 4: Commit smoke-test confirmation (no code changes — just a note commit is not needed)**

No files changed in this task. If Task 1 and Task 2 both pass, the implementation is complete.

---

## Opening the Devcontainer

Once the plan is complete, open in VS Code:

1. Open the Command Palette (`Ctrl+Shift+P`)
2. Run **Dev Containers: Reopen in Container**
3. VS Code builds the image (first time is slow), mounts `/opt/fpp`, installs extensions, and runs `postStartCommand`
4. Open a terminal inside VS Code and run `cd /opt/fpp/src && make -j$(nproc)` to build FPP
5. Use the **Run and Debug** panel — the existing `fppd (gdb) Foreground` config works immediately

## UDP Port Access

UDP ports (DDP 4048, E1.31 5568, Multisync 32320) are not TCP-forwardable by VS Code. To reach them from the host, either:
- Use `docker ps` to find the container ID and inspect its IP, then target that IP directly from E1.31/DDP senders on the same machine
- Or add a `runArgs` entry to `devcontainer.json` to use host networking: `"runArgs": ["--network=host"]` (Linux only, gives container access to all host interfaces)

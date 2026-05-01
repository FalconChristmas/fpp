# Devcontainer Design

**Date:** 2026-05-01
**Status:** Approved

## Goal

Add a VS Code devcontainer configuration to FPP so developers can open the repo and get a fully working build and debug environment inside a container, without disturbing any existing `.vscode/` configuration.

## File Structure

```
.devcontainer/
  devcontainer.json
```

No changes to any existing files. `.vscode/` is left completely intact.

## Container Build & Workspace

- **Base Dockerfile:** `../Docker/Dockerfile` (debian:trixie with full FPP package list)
- **Build args:**
  - `EXTRA_INSTALL_FLAG=--skip-clone` — uses local source during image build instead of cloning from git
  - `FPPBRANCH=master` — default branch label passed to FPP_Install.sh
- **workspaceFolder:** `/opt/fpp` — where FPP expects to live
- **workspaceMount:** repo root mounted into container at `/opt/fpp`, matching `docker-compose-dev.yml`
- **remoteUser:** `root` — FPP runs as root per the existing container setup

The Dockerfile's ENTRYPOINT is ignored by devcontainer. A `postStartCommand` starts the full FPP stack after VS Code connects:
1. `/opt/fpp/src/fppinit start`
2. `/opt/fpp/scripts/fppd_start`
3. `mkdir -p /run/php && /usr/sbin/php-fpm8.4 --fpm-config /etc/php/8.4/fpm/php-fpm.conf`
4. `/usr/sbin/apache2ctl start`

## VS Code Extensions

Added via `devcontainer.json` `customizations.vscode.extensions`:

- `ms-vscode.cpptools` — C/C++ IntelliSense and GDB debug adapter
- `ms-vscode.cpptools-extension-pack` — CMake tools, themes
- `xaver.clang-format` — respects existing `.clang-format` config

Existing `.vscode/extensions.json` is not modified.

## Debug

GDB is already installed in the container (`gdb` package). The existing `.vscode/launch.json` attach configs work as-is once VS Code connects to the devcontainer. No changes needed.

## Port Forwarding

Matching the Dockerfile `EXPOSE` directives:

| Host  | Container | Protocol | Purpose       |
|-------|-----------|----------|---------------|
| 8080  | 80        | TCP      | HTTP/Web UI   |
| 4048  | 4048      | UDP      | DDP           |
| 5568  | 5568      | UDP      | E1.31         |
| 32320 | 32320     | UDP      | Multisync     |
| 32322 | 32322     | TCP      | FPPD/HTTP     |

## What Is Not Changing

- `Docker/Dockerfile` — unchanged
- `Docker/docker-compose-dev.yml` — unchanged
- `.vscode/` — all files unchanged
- All other project files — unchanged

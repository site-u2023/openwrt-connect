# Contributing to OpenWrt Connect

🌐 [日本語](CONTRIBUTING.md) | **English**

## Architecture

```
openwrt-connect.exe              openwrt-connect.ini    openwrt-connect.conf
(Generic Core)                   (Build Settings)       (Runtime Config)
┌─────────────────────┐          ┌────────────────┐     ┌──────────────────────┐
│ ■ IPv4 Gateway      │          │ [general]      │     │ [general]            │
│   Auto-detection    │          │ product_name   │     │ default_ip, ssh_user │
│ ■ SSH Key Auth      │          │                │     │ ssh_key_prefix       │
│   Auto-setup        │          │ [command.*]    │     │                      │
│   dropbear/openssh  │  ←────  │ label, icon    │     │ [command.*]          │
│   Auto-detection    │          └────────────────┘     │ script, url, cmd     │
│ ■ .conf parsing     │                                 └──────────────────────┘
│ ■ Multi-line script │  ←──────────────────────────────────────┘
│ ■ Arg dispatch      │
└─────────────────────┘
```

## Build Flow

```
openwrt-connect-build.bat
  │
  ├─ gcc: openwrt-connect.c → openwrt-connect.exe
  │
  ├─ PowerShell: .ini → Product.wxs (auto-generated)
  │    generate-wxs.ps1
  │      ├─ [general] → Product name, directory name
  │      ├─ [command.*] → Feature, shortcuts
  │      └─ icon → Icon declaration
  │
  └─ WiX: Product.wxs → openwrt-connect.msi
       ├─ openwrt-connect.exe (bundled)
       └─ .conf (bundled: runtime config)
```

## File Structure

| File | Description | Usage |
|---|---|---|
| `*.ini` | Build settings (shortcuts, icons) | Build time only |
| `*.conf` | Runtime config (SSH, command definitions) | EXE runtime |
| `openwrt-connect.c` | Main source (generic core) | |
| `openwrt-connect.rc` | Resource definition | |
| `generate-wxs.ps1` | .ini → Product.wxs generator | |
| `openwrt-connect-build.bat` | Build script | |
| `Product.wxs` | **Auto-generated** (do not edit directly) | |
| `app.manifest` | UAC administrator privilege request | |
| `wix-eula.rtf` | License | |
| `*.ico` | Icon files | |

## Command Definitions (.conf)

### Execution Fields (Priority: script > url > cmd)

| Field | Behavior | Example |
|---|---|---|
| `script` | Execute shell script | Inline multi-line or `./file.sh` |
| `url` | wget remote script and execute | `url = https://example.com/script.sh` |
| `cmd` | Execute single command directly | `cmd = opkg update` |
| (none) | Interactive SSH session | |

### script Field Syntax

**Inline (multi-line):**
```ini
[command.mysetup]
script =
  #!/bin/sh
  echo "Hello"
  opkg update
```

**External file reference (same directory as EXE):**
```ini
[command.adguard]
script = ./adguardhome.sh
```

## Fork / Customization

1. Define app name and shortcuts in `.ini` file

> The `.ini` and `.conf` filenames are flexible (e.g., `myrouter.ini` + `myrouter.conf`).
> The EXE auto-detects the first `.conf` in its directory.

2. Define commands in `.conf` file

3. Build with `openwrt-connect-build.bat`

## Build

### Requirements

- MinGW-w64 (`C:\mingw64\bin` or in PATH)
- WiX Toolset v3.11 (MSI build only)
- PowerShell 5.0+ (Product.wxs auto-generation)

### Steps

```bat
openwrt-connect-build.bat
```

This generates:

- `openwrt-connect.exe` - Executable
- `openwrt-connect.msi` - Installer

## Icons

| File | Usage |
|---|---|
| `openwrt-connect.ico` | EXE + SSH shortcut |

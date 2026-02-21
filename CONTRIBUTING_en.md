# Contributing to OpenWrt Connect

🌐 [日本語](CONTRIBUTING.md) | **English**

## Architecture

```
openwrt-connect.exe              openwrt-connect.conf
(Generic Core)                   (Project-specific)
┌─────────────────────┐          ┌──────────────────────────┐
│ ■ IPv4 Gateway      │          │ [general]                │
│   Auto-detection    │          │ product_name, default_ip │
│ ■ SSH Key Auth      │  ←────  │ ssh_user, ssh_key_prefix │
│   Auto-setup        │          │                          │
│   dropbear/openssh  │          │ [command.xxx]            │
│   Auto-detection    │          │ url, dir, bin, label     │
│ ■ .conf parsing     │          │                          │
│ ■ Template expand   │          │ [command.ssh]            │
│ ■ Arg dispatch      │          │ label (SSH only)         │
└─────────────────────┘          └──────────────────────────┘
```

## Build Flow

```
openwrt-connect-build.bat
  │
  ├─ gcc: openwrt-connect.c → openwrt-connect.exe
  │
  ├─ PowerShell: openwrt-connect.conf → Product.wxs (auto-generated)
  │    generate-wxs.ps1
  │      ├─ [general] → Product name, directory name
  │      ├─ [command.*] → Feature, shortcuts
  │      └─ icon → Icon declaration
  │
  └─ WiX: Product.wxs → openwrt-connect.msi
       ├─ openwrt-connect.exe (bundled)
       └─ openwrt-connect.conf (bundled)
```

## Fork / Customization

This tool can be freely customized as your own script launcher.

1. Change the app name in the `[general]` section of your `.conf` file
> The `.conf` filename is flexible (e.g., `myrouter.conf`). The EXE auto-detects the first `.conf` in its directory.

```ini
[general]
product_name = MyRouter
```

2. Add your own commands

```ini
[command.mysetup]
label = My Custom Script
icon = mysetup.ico
url = https://example.com/my-script.sh
dir = /tmp/mysetup
bin = /usr/bin/mysetup

[command.ssh]
label = SSH Connection
icon = openwrt-connect.ico
```

3. Build

Place your own icon (`.ico`) files and they will be automatically included in the installer.

## Adding Commands

1. Add a `[command.<name>]` section to `openwrt-connect.conf`
2. Place the corresponding `.ico` file (optional)
3. Run `openwrt-connect-build.bat` → automatically reflected in EXE + MSI

## Build

> Required only when building from source.

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

## File List

| File | Description | Editable |
|---|---|---|
| `openwrt-connect.conf` | Command definitions (project-specific) | ○ |
| `openwrt-connect.c` | Main source (generic core) | |
| `openwrt-connect.rc` | Resource definition | |
| `generate-wxs.ps1` | .conf → Product.wxs generator | |
| `openwrt-connect-build.bat` | Build script | |
| `Product.wxs` | **Auto-generated** (do not edit directly) | |
| `app.manifest` | UAC administrator privilege request | |
| `license.rtf` | License | |
| `*.ico` | Icon files | |

## Icons

| File | Usage |
|---|---|
| `openwrt-connect.ico` | EXE + SSH shortcut |

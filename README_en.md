# OpenWrt Connect

🌐 [日本語](README.md) | **English**

## Overview

A launcher tool for `SSH` connection to `OpenWrt` devices and script execution from `Windows`.

- **Featured on Madonomori:** [Connect to OpenWrt routers from Windows - "OpenWrt Connect" v1.0.0](https://forest.watch.impress.co.jp/docs/digest/2086650.html)

## Features

- **Router Auto-detection**: Automatically detects OpenWrt routers on the local network
- **SSH Key Auto-setup**: Automatically generates and configures SSH key pairs
- **Customizable**: Define custom commands via configuration file
- **SSH Package Auto-detection**: Automatically detects Dropbear or OpenSSH
- **Generic Design**: Flexible design for forking and customization

## Requirements

- Windows 10 / 11 (64bit)
- OpenWrt 21.02+ (Dropbear / OpenSSH supported)

## Installation

1. Download `openwrt-connect.msi` from [Releases](https://github.com/site-u2023/openwrt-connect/releases)
2. Run the installer
3. Launch "OpenWrt Connect" from the Start menu

## Usage

### Basic SSH Connection

1. Launch the tool
2. Confirm the auto-detected OpenWrt device IP address (enter manually to change)
3. Enter root password on first connection only (press Enter if no password is set)
4. SSH key is configured automatically
5. Password-free connection from the next time onwards

### How IP Address Auto-detection Works

Retrieves the default gateway from the Windows routing table
```cmd
route print 0.0.0.0
```
> Fallback: `192.168.1.1`

### How SSH Key Authentication Works

**Windows side**

Key generation
```cmd
ssh-keygen -t rsa -f "%USERPROFILE%\.ssh\owrt-connect_<IP>_rsa"
```

Generated key files
```PowerShell
%USERPROFILE%\.ssh\owrt-connect_<IP>_rsa
%USERPROFILE%\.ssh\owrt-connect_<IP>_rsa.pub
```

Key transfer
```cmd
type "%USERPROFILE%\.ssh\owrt-connect_<IP>_rsa.pub" | ssh root@<IP> "cat >> /etc/dropbear/authorized_keys"
```
> Completes public key transfer and SSH connection in a single command

**OpenWrt side**

Deployed key files
```sh
# Dropbear
/etc/dropbear/authorized_keys
# OpenSSH
/root/.ssh/authorized_keys
```

### Adding Custom Commands

Edit `openwrt-connect.conf` to define custom commands.

#### Example: Running a custom script

```ini
[command.mysetup]
label = My Custom Setup
icon = mysetup.ico
url = https://example.com/my-script.sh
dir = /tmp/mysetup
bin = /usr/bin/mysetup
```

This configuration:

1. Runs with `openwrt-connect.exe mysetup`
2. OpenWrt device downloads `https://example.com/my-script.sh`
3. Extracts and runs in `/tmp/mysetup`
4. Persists script to `/usr/bin/mysetup`

#### Example: SSH only (interactive mode)

```ini
[command.terminal]
label = Terminal
icon = terminal.ico
```

If `url` is not specified, opens an interactive SSH session.

## Configuration

### openwrt-connect.conf

| Section | Description |
|---|---|
| `[general]` | App name, default IP, SSH user, key prefix |
| `[command.<name>]` | Command definition (multiple allowed) |

### Command Definition Fields

| Field | Description | Required |
|---|---|---|
| `label` | Display name | ○ |
| `icon` | Icon filename | |
| `url` | Remote script URL | |
| `dir` | Temp directory on device | |
| `bin` | Persistent path on device | |

## Security

### What this tool does

- Detects OpenWrt devices on the local network (routing table lookup)
- Generates SSH key pairs (in user's `.ssh` folder)
- Sends public key to device (password authentication on first connection only)
- Subsequent connections use key authentication

### What this tool does NOT do

- Send information over the internet
- Collect user data
- Communicate with external servers (the EXE itself makes no external connections)

### About script execution

Scripts specified in the `url` field are downloaded and executed by the **OpenWrt device** via `wget`. The EXE itself makes no external connections.

## License

MIT License

## Support

Issues and Pull Requests are welcome.

- [Issues](https://github.com/site-u2023/openwrt-connect/issues)

## Links

- [Contributing](CONTRIBUTING_en.md)
- [OpenWrt SSH Key Authentication Windows App](https://qiita.com/site_u/items/9111335bcbacde4b9ae5)

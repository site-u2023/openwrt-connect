# Contributing to OpenWrt Connect

🌐 **日本語** | [English](CONTRIBUTING_en.md)

## アーキテクチャ

```
openwrt-connect.exe              openwrt-connect.ini    openwrt-connect.conf
(汎用コア)                        (ビルド設定)            (実行設定)
┌─────────────────────┐          ┌────────────────┐     ┌──────────────────────┐
│ ■ IPv4ゲートウェイ   │          │ [general]      │     │ [general]            │
│   自動検出           │          │ product_name   │     │ default_ip, ssh_user │
│ ■ SSH鍵認証          │          │                │     │ ssh_key_prefix       │
│   自動セットアップ   │          │ [command.*]    │     │                      │
│   dropbear/openssh   │  ←────  │ label, icon    │     │ [command.*]          │
│   自動対応           │          └────────────────┘     │ script, url, cmd     │
│ ■ .conf読み込み      │                                 └──────────────────────┘
│ ■ 引数ディスパッチ   │
└─────────────────────┘  ←──────────────────────────────────────┘

```

## ビルドフロー

```
openwrt-connect-build.bat
  │
  ├─ gcc: openwrt-connect.c → openwrt-connect.exe
  │
  ├─ PowerShell: .ini → Product.wxs (自動生成)
  │    generate-wxs.ps1
  │      ├─ [general] → Product名, ディレクトリ名
  │      ├─ [command.*] → Feature, ショートカット
  │      └─ icon → Icon宣言
  │
  └─ WiX: Product.wxs → openwrt-connect.msi
       ├─ openwrt-connect.exe (同梱)
       └─ .conf (同梱: 実行設定)
```

## ファイル構成

| ファイル | 説明 | 用途 |
|---|---|---|
| `*.ini` | ビルド設定（ショートカット、アイコン） | ビルド時のみ |
| `*.conf` | 実行設定（SSH、コマンド定義） | EXE実行時 |
| `openwrt-connect.c` | メインソース（汎用コア） | |
| `openwrt-connect.rc` | リソース定義 | |
| `generate-wxs.ps1` | .ini → Product.wxs 生成 | |
| `openwrt-connect-build.bat` | ビルドスクリプト | |
| `Product.wxs` | **自動生成** (直接編集不要) | |
| `app.manifest` | UAC管理者権限要求 | |
| `wix-eula.rtf` | ライセンス | |
| `*.ico` | アイコン各種 | |

## コマンド定義（.conf）

### 実行フィールド（優先順位: script > url > cmd）

| フィールド | 動作 | 例 |
|---|---|---|
| `script` | シェルスクリプト実行 | `./file.sh` |
| `url` | リモートスクリプトをwgetして実行 | `url = https://example.com/script.sh` |
| `cmd` | 単一コマンドを直接実行 | `cmd = opkg update` |
| (なし) | インタラクティブSSHセッション | |

### script フィールドの書き方

**外部ファイル参照（EXEと同ディレクトリ）:**
```ini
[command.adguard]
script = ./adguardhome.sh
```

## フォーク・カスタマイズ

1. `.ini`ファイルでアプリ名とショートカットを定義

> `.ini`と`.conf`のファイル名は自由です（例：`myrouter.ini` + `myrouter.conf`）。
> EXEは同じディレクトリの最初の`.conf`を自動検出します。

2. `.conf`ファイルでコマンドを定義

3. `openwrt-connect-build.bat`でビルド

## ビルド方法

### 必要ツール

- MinGW-w64 (`C:\mingw64\bin` または PATH上)
- WiX Toolset v3.11 (MSIビルド時のみ)
- PowerShell 5.0+ (Product.wxs自動生成)

### 手順

```bat
openwrt-connect-build.bat
```

これにより以下が生成されます：

- `openwrt-connect.exe` - 実行ファイル
- `openwrt-connect.msi` - インストーラー

## アイコン

| ファイル | 用途 |
|---|---|
| `openwrt-connect.ico` | EXE本体 + SSHショートカット |

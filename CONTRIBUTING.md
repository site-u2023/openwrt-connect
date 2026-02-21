# Contributing to OpenWrt Connect

🌐 **日本語** | [English](CONTRIBUTING_en.md)

## アーキテクチャ

```
openwrt-connect.exe              openwrt-connect.conf
(汎用コア)                        (固有設定)
┌─────────────────────┐          ┌──────────────────────────┐
│ ■ IPv4ゲートウェイ   │          │ [general]                │
│   自動検出           │          │ product_name, default_ip │
│ ■ SSH鍵認証          │  ←────  │ ssh_user, ssh_key_prefix │
│   自動セットアップ   │          │                          │
│   dropbear/openssh   │          │ [command.xxx]            │
│   自動対応           │          │ url, dir, bin, label     │
│ ■ .conf読み込み      │          │                          │
│ ■ テンプレート展開   │          │ [command.ssh]            │
│ ■ 引数ディスパッチ   │          │ label (SSH only)         │
└─────────────────────┘          └──────────────────────────┘
```

## ビルドフロー

```
openwrt-connect-build.bat
  │
  ├─ gcc: openwrt-connect.c → openwrt-connect.exe
  │
  ├─ PowerShell: openwrt-connect.conf → Product.wxs (自動生成)
  │    generate-wxs.ps1
  │      ├─ [general] → Product名, ディレクトリ名
  │      ├─ [command.*] → Feature, ショートカット
  │      └─ icon → Icon宣言
  │
  └─ WiX: Product.wxs → openwrt-connect.msi
       ├─ openwrt-connect.exe (同梱)
       └─ openwrt-connect.conf (同梱)
```

## フォーク・カスタマイズ

このツールは、独自のスクリプトランチャーとして自由にカスタマイズできます。

1. `.conf`ファイルの`[general]`セクションでアプリ名を変更
> `.conf`ファイル名は自由です（例：`myrouter.conf`）。EXEは同じディレクトリの最初の`.conf`を自動検出します。

```ini
[general]
product_name = MyRouter
```

2. 独自のコマンドを追加

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

3. ビルド

独自のアイコン（`.ico`）を配置すれば、インストーラーに自動的に含まれます。

## コマンド追加手順

1. `openwrt-connect.conf`に`[command.新コマンド名]`セクション追加
2. 対応する`.ico`ファイルを配置（任意）
3. `openwrt-connect-build.bat`実行 → EXE + MSI に自動反映

## ビルド方法

> ソースからビルドする場合のみ必要です。

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

## ファイル一覧

| ファイル | 説明 | 編集対象 |
|---|---|---|
| `openwrt-connect.conf` | コマンド定義（固有設定） | ○ |
| `openwrt-connect.c` | メインソース（汎用コア） | |
| `openwrt-connect.rc` | リソース定義 | |
| `generate-wxs.ps1` | .conf → Product.wxs 生成 | |
| `openwrt-connect-build.bat` | ビルドスクリプト | |
| `Product.wxs` | **自動生成** (直接編集不要) | |
| `app.manifest` | UAC管理者権限要求 | |
| `license.rtf` | ライセンス | |
| `*.ico` | アイコン各種 | |

## アイコン

| ファイル | 用途 |
|---|---|
| `openwrt-connect.ico` | EXE本体 + SSHショートカット |

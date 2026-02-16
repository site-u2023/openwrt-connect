# OpenWrt Connect

Windows から OpenWrt デバイスに簡単に SSH 接続し、カスタムスクリプトを実行できるツール。

## 特徴

- **自動検出**: ローカルネットワーク内の OpenWrt ルーターを自動検出
- **鍵認証**: SSH 鍵ペアの自動生成と設定
- **カスタマイズ**: 設定ファイルで独自コマンドを定義可能
- **両対応**: Dropbear / OpenSSH 自動判別
- **汎用設計**: フォーク・カスタマイズを前提とした柔軟な設計

## インストール

1. [Releases](https://github.com/site-u2023/openwrt-connect/releases) から `openwrt-connect.msi` をダウンロード
2. インストーラーを実行
3. スタートメニューから「OpenWrt Connect」を起動

## 使い方

### 基本的な SSH 接続

1. ツールを起動
2. OpenWrt デバイスの IP アドレスを入力（未入力の場合は自動検出）
3. 初回のみ root パスワードを入力（SSH 鍵を自動設定）
4. 次回以降はパスワード不要で接続

### カスタムコマンドの追加

`openwrt-connect.conf` を編集して、独自のコマンドを定義できます。

#### 例: カスタムスクリプトの実行

```ini
[command.mysetup]
label = My Custom Setup
icon = mysetup.ico
url = https://example.com/my-script.sh
dir = /tmp/mysetup
bin = /usr/bin/mysetup
```

この設定により：
1. `openwrt-connect.exe mysetup` で実行
2. OpenWrt デバイスが `https://example.com/my-script.sh` をダウンロード
3. `/tmp/mysetup` に展開して実行
4. スクリプトを `/usr/bin/mysetup` に永続化

#### 例: SSH のみ（インタラクティブモード）

```ini
[command.terminal]
label = Terminal
icon = terminal.ico
```

`url` が未指定の場合、対話型 SSH セッションを開きます。

## 設定ファイル

### openwrt-connect.conf

| セクション | 説明 |
|---|---|
| `[general]` | 製品名、デフォルト IP、SSH ユーザー、鍵プレフィックス |
| `[command.<名前>]` | コマンド定義（複数定義可能） |

### コマンド定義のフィールド

| フィールド | 説明 | 必須 |
|---|---|---|
| `label` | 表示名 | ○ |
| `icon` | アイコンファイル名 | |
| `url` | リモートスクリプト URL | |
| `dir` | デバイス上の一時ディレクトリ | |
| `bin` | デバイス上の永続化パス | |

## アーキテクチャ

```
openwrt-connect.exe         openwrt-connect.conf
(汎用コア)                   (固有設定)
┌─────────────────┐         ┌──────────────────┐
│ ゲートウェイ検出 │  ←────  │ [general]        │
│ SSH鍵認証       │         │ [command.xxx]    │
│ 設定ファイル解析 │         │ [command.yyy]    │
│ スクリプト実行   │         └──────────────────┘
└─────────────────┘
```

## ビルド方法

### 必要ツール

- MinGW-w64
- WiX Toolset v3.11
- PowerShell 5.0+

### 手順

```bat
openwrt-connect-build.bat
```

これにより以下が生成されます：
- `openwrt-connect.exe` - 実行ファイル
- `openwrt-connect.msi` - インストーラー

## フォーク・カスタマイズ

このツールは、独自のスクリプトランチャーとして自由にカスタマイズできます。

### カスタマイズ例

1. `openwrt-connect.conf` の `[general]` セクションを変更

```ini
[general]
product_name = My Router Tool
```

2. 独自のコマンドを追加

```ini
[command.mycommand]
label = My Command
icon = mycommand.ico
url = https://myserver.com/script.sh
dir = /tmp/mycommand
bin = /usr/bin/mycommand
```

3. ビルド

```bat
openwrt-connect-build.bat
```

独自のアイコン (`.ico`) を配置すれば、インストーラーに自動的に含まれます。

## セキュリティ

### このツールが行うこと

- ローカルネットワーク内の OpenWrt デバイスを検出（ARP テーブル参照）
- SSH 鍵ペアの生成（ユーザーの `.ssh` フォルダ内）
- 公開鍵をデバイスに送信（初回のみパスワード認証経由）
- 以降、鍵認証でコマンド実行

### このツールが行わないこと

- インターネット経由での情報送信
- ユーザーデータの収集
- 外部サーバーへの通信（EXE 自体は通信しません）

### スクリプト実行について

`url` フィールドで指定したスクリプトは、**OpenWrt デバイス側**が `wget` でダウンロードして実行します。EXE 自体は外部通信を行いません。

## ライセンス

MIT License

## 貢献

Issue や Pull Request を歓迎します。

## サポート

- GitHub Issues: [https://github.com/site-u2023/openwrt-connect/issues](https://github.com/site-u2023/openwrt-connect/issues)

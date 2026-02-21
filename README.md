# OpenWrt Connect

🌐 **日本語** | [English](README_en.md)

## 概要

`Windows`から`OpenWrt`デバイスに簡単`SSH`接続し、カスタムスクリプトを実行できるツール。

- **窓の杜紹介**：[OSに「OpenWrt」を使ったルーターへWindowsから接続「OpenWrt Connect」v1.0.0　ほか](https://forest.watch.impress.co.jp/docs/digest/2086650.html)

## 特徴

- **ルーター自動検出**：ローカルネットワーク内のOpenWrtルーターを自動検出
- **鍵認証自動設定**：SSH鍵ペアを自動で生成・設定
- **柔軟なコマンド定義**：script（インライン/外部ファイル）、url、cmdの3方式に対応
- **SSHパッケージ自動判別**：DropbearとOpenSSHを自動で判別
- **設定ファイル分離**：ビルド設定（.ini）と実行設定（.conf）を分離した明確な設計

## 動作環境

- Windows 10 / 11 (64bit)
- OpenWrt 21.02+ (Dropbear / OpenSSH 対応)

## インストール

1. [Releases](https://github.com/site-u2023/openwrt-connect/releases)から`openwrt-connect.msi`をダウンロード
2. インストーラーを実行
3. スタートメニューから「OpenWrt Connect」を起動

## 使い方

### 基本的なSSH接続

1. ツールを起動
2. 自動検出したOpenWrtデバイスのIPアドレスを確認（変更する場合は手動入力）
3. 初回のみrootパスワードを入力（パスワード未設定の場合は空エンター）
4. SSH鍵を自動設定
5. 次回以降はパスワード不要で接続

### IPアドレス自動検出の仕組み

Windowsルーティングテーブルからデフォルトゲートウェイを取得
```cmd
route print 0.0.0.0
```
> フォールバック：`192.168.1.1`

### SSH鍵認証の仕組み

**Windows側**

鍵の生成
```cmd
ssh-keygen -t rsa -f "%USERPROFILE%\.ssh\openwrt-connect_<IP>_rsa"
```

生成される鍵ファイル
```PowerShell
%USERPROFILE%\.ssh\openwrt-connect_<IP>_rsa
%USERPROFILE%\.ssh\openwrt-connect_<IP>_rsa.pub
```

鍵の転送
```cmd
type "%USERPROFILE%\.ssh\openwrt-connect_<IP>_rsa.pub" | ssh root@<IP> "cat >> /etc/dropbear/authorized_keys"
```
> 公開鍵の転送とSSH接続を1コマンドで完結

**OpenWrt側**

配置される鍵ファイル
```sh
# Dropbear
/etc/dropbear/authorized_keys
# OpenSSH
/root/.ssh/authorized_keys
```

### コマンド定義

`.conf`ファイルで3種類のコマンド実行方式を定義できます。

#### script - シェルスクリプト実行

**インライン（複数行）：**
```ini
[command.setup]
script =
  #!/bin/sh
  opkg update
  opkg install luci-i18n-base-ja
```

**外部ファイル参照（EXEと同ディレクトリ）：**
```ini
[command.adguard]
script = ./adguardhome.sh
```

#### url - リモートスクリプト実行

```ini
[command.mysetup]
url = https://example.com/my-script.sh
```

#### cmd - 単一コマンド実行

```ini
[command.update]
cmd = opkg update && opkg upgrade luci
```

#### SSH接続のみ

```ini
[command.ssh]
```

フィールドなし = インタラクティブSSHセッション。

## 設定ファイル

### v2.0.0 での変更点

ビルド設定と実行設定を分離しました：

| ファイル | 用途 | 読み込むもの |
|---|---|---|
| `*.ini` | ビルド設定（ショートカット、アイコン） | generate-wxs.ps1 |
| `*.conf` | 実行設定（SSH、コマンド定義） | openwrt-connect.exe |

### .ini（ビルド設定）

| セクション | 説明 |
|---|---|
| `[general]` | アプリ名 |
| `[command.<名前>]` | ショートカット定義（label, icon） |

### .conf（実行設定）

| セクション | 説明 |
|---|---|
| `[general]` | デフォルトIP、SSHユーザー、鍵プレフィックス |
| `[command.<名前>]` | コマンド定義（script, url, cmd） |

### コマンド定義のフィールド

**ビルド設定（.ini）：**

| フィールド | 説明 | 必須 |
|---|---|---|
| `label` | 表示名 | ○ |
| `icon` | アイコンファイル名 | |

**実行設定（.conf）：**

| フィールド | 説明 | 優先順位 |
|---|---|---|
| `script` | シェルスクリプト（インライン or `./file.sh`） | 1 |
| `url` | リモートスクリプトURL | 2 |
| `cmd` | 単一コマンド | 3 |

## セキュリティ

### このツールが行うこと

- ローカルネットワーク内のOpenWrtデバイスを検出（ルーティングテーブル参照）
- SSH鍵ペアの生成（ユーザーの`.ssh`フォルダ内）
- 公開鍵をデバイスに送信（初回のみパスワード認証経由）
- 以降、鍵認証でコマンド実行

### このツールが行わないこと

- インターネット経由での情報送信
- ユーザーデータの収集
- 外部サーバーへの通信（EXE自体は通信しません）

### スクリプト実行について

`url`フィールドで指定したスクリプトは、**OpenWrtデバイス側**が`wget`でダウンロードして実行します。EXE自体は外部通信を行いません。

## ライセンス

MIT License

## サポート

IssueやPull Requestを歓迎します。

- [Issues](https://github.com/site-u2023/openwrt-connect/issues)

## リンク

- [Contributing](CONTRIBUTING.md)
- [OpenWrt SSH 鍵認証 Windows アプリ](https://qiita.com/site_u/items/9111335bcbacde4b9ae5)

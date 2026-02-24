# OpenWrt Connect

🌐 **日本語** | [English](README_en.md)

## 概要

`Windows`が`OpenWrt`デバイスに簡単`SSH`接続し、カスタムスクリプトを実行できるツール。

- **窓の杜紹介**：[OSに「OpenWrt」を使ったルーターへWindowsから接続「OpenWrt Connect」v1.0.0　ほか](https://forest.watch.impress.co.jp/docs/digest/2086650.html)

## 特徴

- **ルーター自動検出**：ローカルネットワーク内のOpenWrtルーターを自動検出
- **鍵認証自動設定**：SSH鍵ペアを自動で生成・設定
- **カスタマイズ性**：設定ファイルで独自コマンドを定義可能
- **SSHパッケージ自動判別**：DropbearとOpenSSHを自動で判別
- **汎用設計**：フォーク・カスタマイズを前提とした柔軟な設計

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
ssh-keygen -t rsa -f "%USERPROFILE%\.ssh\owrt-connect_<IP>_rsa"
```

生成される鍵ファイル
```PowerShell
%USERPROFILE%\.ssh\owrt-connect__rsa
%USERPROFILE%\.ssh\owrt-connect__rsa.pub
```

鍵の転送
```cmd
type "%USERPROFILE%\.ssh\owrt-connect_<IP>_rsa.pub" | ssh root@<IP> "cat >> /etc/dropbear/authorized_keys"
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

### カスタムコマンドの追加

`openwrt-connect.conf`を編集して、独自のコマンドを定義可能

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

1. `openwrt-connect.exe mysetup`で実行
2. OpenWrtデバイスが`https://example.com/my-script.sh`をダウンロード
3. `/tmp/mysetup`に展開して実行
4. スクリプトを`/usr/bin/mysetup`に永続化

#### 例: SSHのみ（インタラクティブモード）

```ini
[command.terminal]
label = Terminal
icon = terminal.ico
```

`url`が未指定の場合、対話型SSHセッションを開きます。

## 設定ファイル

### openwrt-connect.conf

| セクション | 説明 |
|---|---|
| `[general]` | アプリ名、デフォルトIP、SSHユーザー、鍵プレフィックス |
| `[command.<名前>]` | コマンド定義（複数定義可能） |

### コマンド定義のフィールド

| フィールド | 説明 | 必須 |
|---|---|---|
| `label` | 表示名 | ○ |
| `icon` | アイコンファイル名 | |
| `url` | リモートスクリプトURL | |
| `dir` | デバイス上の一時ディレクトリ | |
| `bin` | デバイス上の永続化パス | |

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

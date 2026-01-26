<p align="right"><a href="https://github.com/linyows/octopass/blob/main/README.md">English</a> | 日本語</p>

<br><br><br><br><br><br>

<p align="center">
  <a href="https://octopass.linyo.ws">
    <img alt="OCTOPASS" src="https://raw.githubusercontent.com/linyows/octopass/main/misc/octopass-logo.svg" width="400">
  </a>
  <br><br>
  GitHub Organization/Team で Linux ユーザーを管理
</p>

<br><br><br><br><br>

octopass は GitHub のチーム管理を Linux サーバーに持ち込みます。`/etc/passwd` の手動管理や SSH 鍵の配布はもう不要です。GitHub チームにユーザーを追加するだけで、サーバーに SSH できるようになります。

<br>

<p align="center">
  <a href="https://github.com/linyows/octopass/actions/workflows/build.yml" title="Build"><img alt="GitHub Workflow Status" src="https://img.shields.io/github/actions/workflow/status/linyows/octopass/build.yml?branch=main&style=for-the-badge"></a>
  <a href="https://github.com/linyows/octopass/releases" title="GitHub release"><img src="http://img.shields.io/github/release/linyows/octopass.svg?style=for-the-badge&labelColor=666666&color=DDDDDD" alt="GitHub Release"></a>
</p>

## なぜ octopass？

🔑 **GitHub の SSH 鍵を使用** — ユーザーは GitHub の SSH 鍵で認証します。鍵の配布は不要です。

👥 **チームベースのアクセス** — GitHub チームのメンバーシップでサーバーアクセスを付与。チームに追加 = サーバーアクセス。

🔄 **常に同期** — ユーザーリストと鍵は GitHub API から取得。チームから削除 = アクセス取り消し。

🛡️ **セキュアな設計** — サーバーにパスワードを保存しません。GitHub パーソナルアクセストークンで認証。

📦 **依存関係ゼロ** — 単一の静的バイナリ。libc 以外のランタイム依存なし。

## 仕組み

![Architecture](https://github.com/linyows/octopass/blob/main/misc/architecture.png)

octopass は **NSS (Name Service Switch) モジュール**として動作し、GitHub チームを Linux ユーザー管理にシームレスに統合します：

- `getpwnam()` / `getpwuid()` → GitHub チームメンバーを Linux ユーザーとして返す
- `getgrnam()` / `getgrgid()` → GitHub チームを Linux グループとして返す
- SSH `AuthorizedKeysCommand` → GitHub からユーザーの SSH 公開鍵を取得

## クイックスタート

### 1. インストール

**RHEL/CentOS/Amazon Linux の場合：**

```bash
curl -s https://packagecloud.io/install/repositories/linyows/octopass/script.rpm.sh | sudo bash
sudo yum install octopass
```

**Debian/Ubuntu の場合：**

```bash
curl -s https://packagecloud.io/install/repositories/linyows/octopass/script.deb.sh | sudo bash
sudo apt-get install octopass
```

**ソースからビルド：**

```bash
# Zig 0.15+ が必要
zig build -Doptimize=ReleaseSafe

# NSS ライブラリをインストール
sudo cp zig-out/lib/libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/
sudo ln -sf libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/libnss_octopass.so.2

# CLI をインストール
sudo cp zig-out/bin/octopass /usr/bin/
```

### 2. 設定

`/etc/octopass.conf` を作成：

```ini
# GitHub パーソナルアクセストークン（read:org スコープが必要）
Token = "ghp_xxxxxxxxxxxxxxxxxxxx"

# GitHub Organization
Organization = "your-org"

# アクセスを許可するチーム（チームスラッグ）
Team = "your-team"

# ユーザー設定
UidStarts = 2000
Gid = 2000
Home = "/home/%s"
Shell = "/bin/bash"

# キャッシュ設定（秒）
Cache = 300
```

### 3. NSS モジュールを有効化

`/etc/nsswitch.conf` を編集：

```
passwd: files octopass
group:  files octopass
shadow: files octopass
```

### 4. SSH を設定

`/etc/ssh/sshd_config` を編集：

```
AuthorizedKeysCommand /usr/bin/octopass %u
AuthorizedKeysCommandUser root
UsePAM yes
PasswordAuthentication no
```

SSH を再起動：

```bash
sudo systemctl restart sshd
```

## 使い方

```bash
# ユーザーの SSH 鍵を取得
octopass alice

# 全ユーザーをリスト（passwd 形式）
octopass passwd

# 特定のユーザーエントリを取得
octopass passwd alice

# グループエントリをリスト
octopass group

# PAM 認証（stdin からトークンを読み取り）
echo $GITHUB_TOKEN | octopass pam alice
```

## 設定オプション

| オプション | 説明 | デフォルト |
|-----------|------|-----------|
| `Token` | GitHub パーソナルアクセストークン | (必須) |
| `Organization` | GitHub Organization 名 | (必須) |
| `Team` | GitHub チームスラッグ | (チームモードでは必須) |
| `Owner` | リポジトリオーナー（コラボレーターモード用） | - |
| `Repository` | リポジトリ名（コラボレーターモード用） | - |
| `Permission` | 必要な権限: `read`, `write`, `admin` | `write` |
| `Endpoint` | GitHub API エンドポイント | `https://api.github.com/` |
| `UidStarts` | GitHub ユーザーの開始 UID | `2000` |
| `Gid` | チームグループの GID | `2000` |
| `Group` | Linux グループ名 | チーム名 |
| `Home` | ホームディレクトリパターン（`%s` = ユーザー名） | `/home/%s` |
| `Shell` | デフォルトシェル | `/bin/bash` |
| `Cache` | キャッシュ TTL（秒、0 = 無効） | `500` |
| `Syslog` | syslog ロギングを有効化 | `false` |
| `SharedUsers` | 全チームメンバーの鍵を受け入れるユーザー | `[]` |

## リポジトリコラボレーターモード

GitHub チームの代わりに、リポジトリコラボレーターを使用できます：

```ini
Token = "ghp_xxxxxxxxxxxxxxxxxxxx"
Owner = "your-org"
Repository = "your-repo"
Permission = "write"  # write 権限を持つコラボレーターのみ
```

## 共有ユーザー

共有アカウント（`deploy` や `admin` など）では、チームメンバー全員が認証できるようにできます：

```ini
SharedUsers = ["deploy", "admin"]
```

`deploy` として SSH すると、全チームメンバーの SSH 鍵が受け入れられます。

## 環境変数

設定は環境変数で上書きできます：

- `OCTOPASS_TOKEN`
- `OCTOPASS_ENDPOINT`
- `OCTOPASS_ORGANIZATION`
- `OCTOPASS_TEAM`
- `OCTOPASS_OWNER`
- `OCTOPASS_REPOSITORY`

## なぜ Zig？

これはオリジナルの C 実装を Zig で書き直したものです。利点：

- **メモリ安全性** — コンパイル時チェックで一般的な脆弱性を防止
- **依存関係なし** — Zig の標準ライブラリが libcurl と jansson を置き換え
- **簡単なクロスコンパイル** — どのホストからでも任意のターゲット向けにビルド
- **統合テスト** — 組み込みのテストフレームワーク
- **読みやすいコード** — パフォーマンスを犠牲にせず、C よりクリーン

## ライセンス

MIT

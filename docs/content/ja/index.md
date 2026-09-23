---
title: Octopass
description: GitHubのOrganizationのチームで、LinuxのユーザーとSSHのアクセスを管理します。
---

**Octopassは、GitHubのOrganizationのチームで、LinuxのユーザーとSSHのアクセスを管理するツールです。**チームにメンバーを追加すれば、そのメンバーはGitHubに登録済みのSSH鍵でサーバーにログインできます。チームから外せば、アクセスもなくなります。`useradd`も、`authorized_keys`の配布も、LDAPサーバーの運用も要りません。

OctopassはNSSモジュールと小さなCLIとして動きます。`nsswitch.conf`、`sshd`、PAMという標準の仕組みに組み込むだけなので、サーバーの使い方は変わりません。

## 特徴

Octopassを使うと、ユーザーと鍵の管理をサーバーから切り離し、チームをすでに管理しているGitHubに任せられます。これによって、次の利点が得られます。

- **チームがそのままユーザー一覧になる**：GitHubのチームのメンバー、またはリポジトリのコラボレーターがLinuxユーザーになります。GitLabのグループ、サブグループ、プロジェクトにも対応しています。
- **GitHubの鍵でSSHできる**：公開鍵はログインのたびにGitHubから取得します。GitHubで鍵を変えれば、すべてのサーバーに反映されます。
- **入退社の対応がチームの変更だけで済む**：アクセス権はチームの所属に従います。メンバーが抜けても、各サーバーで後始末をする必要はありません。
- **どのサーバーでも同じUIDになる**：UIDはGitHubのユーザーIDから決まります。中央のディレクトリサーバーがなくても、同じ人はどのサーバーでも同じUIDになります。
- **共有アカウントを使える**：`deploy`のようなアカウントに、チームの全員が自分の鍵でログインできます。
- **速い**：APIのレスポンスをディスクにキャッシュするので、名前解決が速く、APIの呼び出し回数も抑えられます。
- **導入が簡単**：libc以外に実行時の依存がない単一のバイナリです。Debian/Ubuntu向けとRHEL互換ディストリビューション向けのパッケージを用意しています。RPMパッケージにはSELinuxポリシーも含まれます。

## 仕組み

ユーザーがSSHでログインするとき、`sshd`はOctopassに2つのことを問い合わせます。そのユーザーが存在するかと、そのユーザーの公開鍵は何かです。Octopassはどちらにも、ローカルのキャッシュを介してGitHubの情報で答えます。

```mermaid
flowchart TD
  user(["ken"]) -- "ssh" --> sshd["sshd"]
  sshd -- "1. kenは存在するか<br/>getpwnam()" --> nss["NSSモジュール<br/>libnss_octopass"]
  sshd -- "2. kenの公開鍵は何か<br/>AuthorizedKeysCommand" --> cli["octopass CLI"]
  nss --> cache[("キャッシュ<br/>/var/cache/octopass")]
  cli --> cache
  cache -- "キャッシュがないとき" --> api["GitHub API<br/>チームのメンバーとユーザーの公開鍵"]
```

たとえば、GitHubのOrganizationの`operators`チームに`ken`を追加したとします。OctopassはNSSを通じて、`ken`をLinuxユーザーとして解決します。UIDは`UidStarts`にGitHubのユーザーIDを足した値で、グループはチームです。

```bash
$ id ken
uid=5458(ken) gid=2000(operators) groups=2000(operators)
```

`ken`がSSHで接続すると、`sshd`は`AuthorizedKeysCommand`でOctopassに公開鍵を問い合わせます。OctopassはGitHubに登録された公開鍵を返します。

```bash
$ octopass ken
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI...
```

こうして`ken`は、サーバー側でアカウントも鍵も用意することなく、すぐにログインできます。

Octopassが提供する機能は次のとおりです。

- `getpwnam()` / `getpwuid()`：チームのメンバーをユーザーとして返す（`passwd`）
- `getgrnam()` / `getgrgid()`：チームをグループとして返す（`group`）
- `getspnam()`：パスワードをロックしたshadowエントリを返す（`shadow`）
- `octopass <user>`：`AuthorizedKeysCommand`向けにSSH公開鍵を返す
- `octopass pam`：Personal Access Tokenによるパスワード認証（任意）

## 動機

Linuxのアカウントを手作業で管理する方法は、サーバーやメンバーが増えると続けられません。かといって、多くのチームにとってLDAPの運用は重すぎます。一方で、誰がどのチームに所属するかはすでにGitHubで管理していて、エンジニアは皆GitHubにSSH鍵を登録しています。

Octopassは、このGitHubの情報を唯一の情報源として使います。ディレクトリサービスになろうとはせず、「メンバーは誰か」と「その公開鍵は何か」にだけ答えるよう、意図して機能を絞っています。それだけで、新しいメンバーの追加も、抜けたメンバーの削除も、GitHubのチームを変更するだけで済むようになります。

## インストール

Octopassは、NSSライブラリの`libnss_octopass`と`octopass`コマンドからなります。どちらもパッケージでインストールするか、ソースからビルドします。

Debian/Ubuntuの場合：

```bash
curl -s https://packagecloud.io/install/repositories/linyows/octopass/script.deb.sh | sudo bash
sudo apt-get install octopass
```

RHEL/Rocky Linux/AlmaLinuxの場合：

```bash
curl -s https://packagecloud.io/install/repositories/linyows/octopass/script.rpm.sh | sudo bash
sudo yum install octopass
```

パッケージは[packagecloud](https://packagecloud.io/linyows/octopass)で配布しています。バイナリは[GitHub Releases](https://github.com/linyows/octopass/releases)から入手できます。

ソースからビルドするには、[Zig](https://ziglang.org/) 0.16以降が必要です。

```bash
git clone https://github.com/linyows/octopass
cd octopass
zig build -Doptimize=ReleaseSafe

sudo cp zig-out/lib/libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/
sudo ln -sf libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/libnss_octopass.so.2
sudo cp zig-out/bin/octopass /usr/bin/
```

## セットアップ

インストールしたら、各サーバーで設定ファイル、NSS、`sshd`、PAM（任意）の4つを設定します。NSSと`sshd`まで設定すれば、チームのメンバーがSSHでログインできるようになります。

### 1. octopass.conf

OrganizationとチームのメンバーシップをOctopassが読めるように、`read:org`スコープを持つ[Personal Access Token](https://github.com/settings/tokens/new)を作成します。そのうえで`/etc/octopass.conf`を書きます。

```ini
Token        = "ghp_xxxxxxxxxxxxxxxxxxxx"
Organization = "your-org"
Team         = "your-team"
```

### 2. NSSwitch

`/etc/nsswitch.conf`で、名前解決にOctopassを使うよう設定します。

```text
passwd: files octopass
group:  files octopass
shadow: files octopass
```

チームのメンバーが解決できることを確認します。

```bash
id your-github-username
```

### 3. SSHD

`/etc/ssh/sshd_config`で、`sshd`が公開鍵をOctopassから取得するよう設定します。

```text
AuthorizedKeysCommand /usr/bin/octopass %u
AuthorizedKeysCommandUser root
UsePAM yes
PasswordAuthentication no
```

```bash
sudo systemctl restart sshd
```

### 4. PAM（任意）

初回ログイン時にホームディレクトリを作成し、Personal Access Tokenをパスワードとして認証できるようにするには、`/etc/pam.d/sshd`を編集します。

```text
auth    requisite  pam_exec.so     quiet expose_authtok /usr/bin/octopass pam
auth    optional   pam_unix.so     not_set_pass use_first_pass nodelay
session required   pam_mkhomedir.so skel=/etc/skel/ umask=0022
```

## 設定

`/etc/octopass.conf`には次のキーを書けます。

| キー | 説明 | デフォルト |
|-----|-------------|---------|
| `Token` | Personal Access Token | （必須） |
| `Organization` | GitHubのOrganization | - |
| `Team` | GitHubのチームのslug | - |
| `Owner` | リポジトリのオーナー（コラボレーターモード） | `Organization` |
| `Repository` | リポジトリ名（コラボレーターモード） | - |
| `Permission` | コラボレーターに求める権限：`read`、`write`、`admin` | `write` |
| `Endpoint` | APIのエンドポイント（GitHub Enterpriseなど） | `https://api.github.com/` |
| `Group` | Linuxのグループ名 | チーム名またはリポジトリ名 |
| `Home` | ホームディレクトリ（`%s`はユーザー名） | `/home/%s` |
| `Shell` | ログインシェル | `/bin/bash` |
| `UidStarts` | GitHubのユーザーIDに足すオフセット | `2000` |
| `Gid` | グループのGID | `2000` |
| `Cache` | キャッシュの有効期間（秒）。`0`で無効 | `500` |
| `Syslog` | syslogに出力する | `false` |
| `SharedUsers` | 全メンバーの鍵を受け付けるアカウント | `[]` |

### リポジトリのコラボレーター

チームの代わりに、リポジトリのコラボレーターのうち指定した権限以上を持つ人をユーザーにできます。

```ini
Token      = "ghp_xxxxxxxxxxxxxxxxxxxx"
Owner      = "your-org"
Repository = "your-repo"
Permission = "write"
```

### 共有ユーザー

`SharedUsers`に書いたアカウントは、全メンバーのSSH鍵を受け付けます。デプロイ用や運用作業用のアカウントに使えます。

```ini
SharedUsers = ["deploy", "admin"]
```

### GitLab

`Provider`を`gitlab`にすると、GitLabのグループ、サブグループ、プロジェクトを使えます。エンドポイントのデフォルトは`https://gitlab.com/api/v4/`です。

```ini
Provider = "gitlab"
Token    = "glpat-xxxxxxxxxxxxxxxxxxxx"
Group    = "your-group"
Subgroup = "your-subgroup"
```

### 環境変数

次の環境変数は設定ファイルの値を上書きします：`OCTOPASS_PROVIDER`、`OCTOPASS_TOKEN`、`OCTOPASS_ENDPOINT`、`OCTOPASS_ORGANIZATION`、`OCTOPASS_TEAM`、`OCTOPASS_OWNER`、`OCTOPASS_REPOSITORY`、`OCTOPASS_PERMISSION`、`OCTOPASS_GROUP`、`OCTOPASS_SUBGROUP`、`OCTOPASS_PROJECT`。

## 使い方

`octopass`コマンドは、Octopassが解決する内容を表示します。セットアップの確認に使えます。

```bash
octopass ken             # kenのSSH公開鍵
octopass passwd          # 全ユーザー（passwd形式）
octopass passwd ken      # kenのpasswdエントリ
octopass group           # グループのエントリ
octopass shadow          # shadowエントリ
octopass -c ./test.conf passwd   # 別の設定ファイルを使う
```

```bash
$ octopass passwd
chun-li:x:14301:2000:managed by octopass:/home/chun-li:/bin/bash
ken:x:5458:2000:managed by octopass:/home/ken:/bin/bash
ryu:x:74049:2000:managed by octopass:/home/ryu:/bin/bash
```

## プロビジョニング

プロビジョニングツールを作ってくれた[@uchida](https://github.com/uchida)、[@hnmx4](https://github.com/hnmx4)、[@hfm](https://github.com/hfm)に感謝します。

- Chef：[linyows/octopass-cookbook](https://github.com/linyows/octopass-cookbook)
- Itamae：[hnmx4/octopass-itamae-cookbook](https://github.com/hnmx4/octopass-itamae-cookbook)
- Ansible：[uchida/ansible-octopass-role](https://github.com/uchida/ansible-octopass-role)
- Puppet：[hfm/puppet-octopass](https://github.com/hfm/puppet-octopass)

## コントリビュート

バグ報告やプルリクエストは[GitHub](https://github.com/linyows/octopass)で受け付けています。OctopassはMITライセンスで公開しています。

---
title: Octopass
description: Manage Linux users and SSH access with your GitHub organization team.
---

**Octopass manages Linux users and SSH access with your GitHub organization team.**
Add someone to the team, and they can log in to your servers with the SSH keys they already registered on GitHub. Remove them from the team, and their access is gone. No `useradd`, no `authorized_keys` to distribute, no LDAP server to run.

Octopass works as an NSS module and a small CLI, so it plugs into the standard Linux mechanisms — `nsswitch.conf`, `sshd`, and PAM — without changing how your servers work.

## Features

Octopass takes the user and key management off your servers and leaves it to GitHub, where your team is already managed. That brings the following benefits.

- **Your team is your user list** — Members of a GitHub team, or collaborators of a repository, become Linux users. GitLab groups, subgroups and projects are supported as well.
- **SSH with GitHub keys** — Public keys are fetched from GitHub at login time. When a user rotates a key on GitHub, every server follows.
- **Instant onboarding and offboarding** — Access follows team membership. There is nothing to clean up on each server when someone leaves.
- **Consistent UIDs everywhere** — A UID is derived from the GitHub user ID, so the same person gets the same UID on every server, without any central directory.
- **Shared accounts** — Let every team member log in as accounts like `deploy` with their own keys.
- **Fast** — API responses are cached on disk, so name lookups stay fast and API calls stay low.
- **Easy to deploy** — A single binary with no runtime dependencies beyond libc. Packages are available for Debian/Ubuntu and RHEL-compatible distributions, with an SELinux policy included.

## How it works

When a user logs in over SSH, `sshd` asks Octopass two questions: whether the user exists, and which public keys the user has. Octopass answers both from GitHub, with a local cache in between.

```mermaid
flowchart TD
  user(["ken"]) -- "ssh" --> sshd["sshd"]
  sshd -- "1. Does ken exist?<br/>getpwnam()" --> nss["NSS module<br/>libnss_octopass"]
  sshd -- "2. What are ken's keys?<br/>AuthorizedKeysCommand" --> cli["octopass CLI"]
  nss --> cache[("Cache<br/>/var/cache/octopass")]
  cli --> cache
  cache -- "on miss" --> api["GitHub API<br/>team members and user keys"]
```

For example, add `ken` to the `operators` team of your GitHub organization. Octopass resolves `ken` as a Linux user through NSS. The UID is `UidStarts` plus the GitHub user ID, and the group is the team.

```bash
$ id ken
uid=5458(ken) gid=2000(operators) groups=2000(operators)
```

When `ken` connects over SSH, `sshd` asks Octopass for the authorized keys through `AuthorizedKeysCommand`, and Octopass returns the public keys registered on GitHub.

```bash
$ octopass ken
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI...
```

So `ken` can log in right away, with no account or key set up on the server.

Under the hood, Octopass provides:

- `getpwnam()` / `getpwuid()` — team members as users (`passwd`)
- `getgrnam()` / `getgrgid()` — the team as a group (`group`)
- `getspnam()` — shadow entries with locked passwords (`shadow`)
- `octopass <user>` — SSH public keys for `AuthorizedKeysCommand`
- `octopass pam` — optional password authentication with a personal access token

## Motivation

Managing Linux accounts by hand does not scale, and running LDAP is too heavy for many teams. Most teams already manage who belongs where on GitHub, and every engineer already has SSH keys registered there.

Octopass uses that as the single source of truth. It intentionally stays simple: it does not try to be a directory service, it only answers "who are the members" and "what are their keys". That is enough to make adding a new member, or removing one who leaves, a matter of changing the team on GitHub.

## Installation

Octopass consists of the NSS library `libnss_octopass` and the `octopass` command. Install both from a package, or build them from source.

For Debian/Ubuntu:

```bash
curl -s https://packagecloud.io/install/repositories/linyows/octopass/script.deb.sh | sudo bash
sudo apt-get install octopass
```

For RHEL/Rocky Linux/AlmaLinux:

```bash
curl -s https://packagecloud.io/install/repositories/linyows/octopass/script.rpm.sh | sudo bash
sudo yum install octopass
```

Packages are provided via [packagecloud](https://packagecloud.io/linyows/octopass), and binaries are available on [GitHub Releases](https://github.com/linyows/octopass/releases).

To build from source, you need [Zig](https://ziglang.org/) 0.16 or later:

```bash
git clone https://github.com/linyows/octopass
cd octopass
zig build -Doptimize=ReleaseSafe

sudo cp zig-out/lib/libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/
sudo ln -sf libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/libnss_octopass.so.2
sudo cp zig-out/bin/octopass /usr/bin/
```

## Setup

After installing, set up the following four things on each server: the configuration file, NSS, `sshd`, and optionally PAM. Once NSS and `sshd` are configured, team members can log in over SSH.

### 1. octopass.conf

Create a [personal access token](https://github.com/settings/tokens/new) that can read organization and team membership (`read:org`), and write `/etc/octopass.conf`:

```ini
Token        = "ghp_xxxxxxxxxxxxxxxxxxxx"
Organization = "your-org"
Team         = "your-team"
```

### 2. NSSwitch

Enable Octopass for name resolution in `/etc/nsswitch.conf`:

```text
passwd: files octopass
group:  files octopass
shadow: files octopass
```

Check that team members are resolved:

```bash
id your-github-username
```

### 3. SSHD

Let `sshd` fetch public keys from Octopass in `/etc/ssh/sshd_config`:

```text
AuthorizedKeysCommand /usr/bin/octopass %u
AuthorizedKeysCommandUser root
UsePAM yes
PasswordAuthentication no
```

```bash
sudo systemctl restart sshd
```

### 4. PAM (optional)

To create home directories on first login, and to allow authentication with a personal access token as the password, edit `/etc/pam.d/sshd`:

```text
auth    requisite  pam_exec.so     quiet expose_authtok /usr/bin/octopass pam
auth    optional   pam_unix.so     not_set_pass use_first_pass nodelay
session required   pam_mkhomedir.so skel=/etc/skel/ umask=0022
```

## Configuration

`/etc/octopass.conf` accepts the following keys.

| Key | Description | Default |
|-----|-------------|---------|
| `Token` | Personal access token | (required) |
| `Organization` | GitHub organization | - |
| `Team` | GitHub team slug | - |
| `Owner` | Repository owner (collaborator mode) | `Organization` |
| `Repository` | Repository name (collaborator mode) | - |
| `Permission` | Required collaborator permission: `read`, `write` or `admin` | `write` |
| `Endpoint` | API endpoint, e.g. for GitHub Enterprise | `https://api.github.com/` |
| `Group` | Linux group name | team or repository name |
| `Home` | Home directory (`%s` is the user name) | `/home/%s` |
| `Shell` | Login shell | `/bin/bash` |
| `UidStarts` | Offset added to the GitHub user ID | `2000` |
| `Gid` | GID of the group | `2000` |
| `Cache` | Cache TTL in seconds, `0` disables it | `500` |
| `Syslog` | Log to syslog | `false` |
| `SharedUsers` | Accounts that accept keys of all members | `[]` |

### Repository collaborators

Instead of a team, collaborators of a repository who have at least the given permission can be users:

```ini
Token      = "ghp_xxxxxxxxxxxxxxxxxxxx"
Owner      = "your-org"
Repository = "your-repo"
Permission = "write"
```

### Shared users

Accounts listed in `SharedUsers` accept the SSH keys of every member. This is useful for deploy or operation accounts:

```ini
SharedUsers = ["deploy", "admin"]
```

### GitLab

Set `Provider` to `gitlab` to use a GitLab group, subgroup or project. The endpoint defaults to `https://gitlab.com/api/v4/`.

```ini
Provider = "gitlab"
Token    = "glpat-xxxxxxxxxxxxxxxxxxxx"
Group    = "your-group"
Subgroup = "your-subgroup"
```

### Environment variables

The following environment variables override the configuration file: `OCTOPASS_PROVIDER`, `OCTOPASS_TOKEN`, `OCTOPASS_ENDPOINT`, `OCTOPASS_ORGANIZATION`, `OCTOPASS_TEAM`, `OCTOPASS_OWNER`, `OCTOPASS_REPOSITORY`, `OCTOPASS_PERMISSION`, `OCTOPASS_GROUP`, `OCTOPASS_SUBGROUP` and `OCTOPASS_PROJECT`.

## Usage

The `octopass` command shows what Octopass resolves, which is handy for checking your setup:

```bash
octopass ken             # SSH public keys of ken
octopass passwd          # all users in passwd format
octopass passwd ken      # passwd entry of ken
octopass group           # group entry
octopass shadow          # shadow entries
octopass -c ./test.conf passwd   # use another config file
```

```bash
$ octopass passwd
chun-li:x:14301:2000:managed by octopass:/home/chun-li:/bin/bash
ken:x:5458:2000:managed by octopass:/home/ken:/bin/bash
ryu:x:74049:2000:managed by octopass:/home/ryu:/bin/bash
```

## Provisioning

Thanks to [@uchida](https://github.com/uchida), [@hnmx4](https://github.com/hnmx4) and [@hfm](https://github.com/hfm) for the provisioning tools.

- Chef: [linyows/octopass-cookbook](https://github.com/linyows/octopass-cookbook)
- Itamae: [hnmx4/octopass-itamae-cookbook](https://github.com/hnmx4/octopass-itamae-cookbook)
- Ansible: [uchida/ansible-octopass-role](https://github.com/uchida/ansible-octopass-role)
- Puppet: [hfm/puppet-octopass](https://github.com/hfm/puppet-octopass)

## Contributing

Bug reports and pull requests are welcome on [GitHub](https://github.com/linyows/octopass). Octopass is released under the MIT License.

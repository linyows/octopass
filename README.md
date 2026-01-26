<p align="right">English | <a href="https://github.com/linyows/octopass/blob/main/README.ja.md">日本語</a></p>

<br><br><br><br><br><br>

<p align="center">
  <a href="https://octopass.linyo.ws">
    <img alt="OCTOPASS" src="https://raw.githubusercontent.com/linyows/octopass/main/misc/octopass-logo.svg" width="400">
  </a>
  <br><br>
  Manage Linux users with your GitHub Organization/Team
</p>

<br><br><br><br><br>

octopass brings GitHub's team management to your Linux servers. No more manually managing `/etc/passwd` or distributing SSH keys — just add users to your GitHub team, and they're ready to SSH into your servers.

<br>

<p align="center">
  <a href="https://github.com/linyows/octopass/actions/workflows/build.yml" title="Build"><img alt="GitHub Workflow Status" src="https://img.shields.io/github/actions/workflow/status/linyows/octopass/build.yml?branch=main&style=for-the-badge"></a>
  <a href="https://github.com/linyows/octopass/releases" title="GitHub release"><img src="http://img.shields.io/github/release/linyows/octopass.svg?style=for-the-badge&labelColor=666666&color=DDDDDD" alt="GitHub Release"></a>
</p>

## Why octopass?

🔑 **SSH keys from GitHub** — Users authenticate with their GitHub SSH keys. No key distribution needed.

👥 **Team-based access** — Grant server access by GitHub team membership. Add to team = server access.

🔄 **Always in sync** — User lists and keys are fetched from GitHub API. Remove from team = access revoked.

🛡️ **Secure by design** — No passwords stored on servers. Authentication via GitHub personal access tokens.

📦 **Zero dependencies** — Single static binary. No runtime dependencies beyond libc.

## How it works

![Architecture](https://github.com/linyows/octopass/blob/main/misc/architecture.png)

octopass works as a **NSS (Name Service Switch) module**, seamlessly integrating GitHub teams into Linux user management:

- `getpwnam()` / `getpwuid()` → Returns GitHub team members as Linux users
- `getgrnam()` / `getgrgid()` → Returns GitHub team as a Linux group
- SSH `AuthorizedKeysCommand` → Fetches user's SSH public keys from GitHub

## Quick Start

### 1. Install

**For RHEL/CentOS/Amazon Linux:**

```bash
curl -s https://packagecloud.io/install/repositories/linyows/octopass/script.rpm.sh | sudo bash
sudo yum install octopass
```

**For Debian/Ubuntu:**

```bash
curl -s https://packagecloud.io/install/repositories/linyows/octopass/script.deb.sh | sudo bash
sudo apt-get install octopass
```

**Build from source:**

```bash
# Requires Zig 0.15+
zig build -Doptimize=ReleaseSafe

# Install the NSS library
sudo cp zig-out/lib/libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/
sudo ln -sf libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/libnss_octopass.so.2

# Install the CLI
sudo cp zig-out/bin/octopass /usr/bin/
```

### 2. Configure

Create `/etc/octopass.conf`:

```ini
# GitHub personal access token (requires read:org scope)
Token = "ghp_xxxxxxxxxxxxxxxxxxxx"

# Your GitHub organization
Organization = "your-org"

# Team to grant access (team slug)
Team = "your-team"

# User configuration
UidStarts = 2000
Gid = 2000
Home = "/home/%s"
Shell = "/bin/bash"

# Cache settings (seconds)
Cache = 300
```

### 3. Enable NSS module

Edit `/etc/nsswitch.conf`:

```
passwd: files octopass
group:  files octopass
shadow: files octopass
```

### 4. Configure SSH

Edit `/etc/ssh/sshd_config`:

```
AuthorizedKeysCommand /usr/bin/octopass %u
AuthorizedKeysCommandUser root
UsePAM yes
PasswordAuthentication no
```

Restart SSH:

```bash
sudo systemctl restart sshd
```

## Usage

```bash
# Get SSH keys for a user
octopass alice

# List all users (passwd format)
octopass passwd

# Get specific user entry
octopass passwd alice

# List group entry
octopass group

# PAM authentication (reads token from stdin)
echo $GITHUB_TOKEN | octopass pam alice
```

## Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `Token` | GitHub personal access token | (required) |
| `Organization` | GitHub organization name | (required) |
| `Team` | GitHub team slug | (required for team mode) |
| `Owner` | Repository owner (for collaborator mode) | - |
| `Repository` | Repository name (for collaborator mode) | - |
| `Permission` | Required permission: `read`, `write`, `admin` | `write` |
| `Endpoint` | GitHub API endpoint | `https://api.github.com/` |
| `UidStarts` | Starting UID for GitHub users | `2000` |
| `Gid` | GID for the team group | `2000` |
| `Group` | Linux group name | team name |
| `Home` | Home directory pattern (`%s` = username) | `/home/%s` |
| `Shell` | Default shell | `/bin/bash` |
| `Cache` | Cache TTL in seconds (0 = disabled) | `500` |
| `Syslog` | Enable syslog logging | `false` |
| `SharedUsers` | Users who get all team members' keys | `[]` |

## Repository Collaborator Mode

Instead of GitHub teams, you can use repository collaborators:

```ini
Token = "ghp_xxxxxxxxxxxxxxxxxxxx"
Owner = "your-org"
Repository = "your-repo"
Permission = "write"  # Only collaborators with write access
```

## Shared Users

For shared accounts (like `deploy` or `admin`), you can allow any team member to authenticate:

```ini
SharedUsers = ["deploy", "admin"]
```

When someone SSHs as `deploy`, all team members' SSH keys are accepted.

## Environment Variables

Configuration can be overridden with environment variables:

- `OCTOPASS_TOKEN`
- `OCTOPASS_ENDPOINT`
- `OCTOPASS_ORGANIZATION`
- `OCTOPASS_TEAM`
- `OCTOPASS_OWNER`
- `OCTOPASS_REPOSITORY`

## Why Zig?

This is a Zig rewrite of the original C implementation. Benefits:

- **Memory safety** — Compile-time checks prevent common vulnerabilities
- **No dependencies** — Zig's stdlib replaces libcurl and jansson
- **Easy cross-compilation** — Build for any target from any host
- **Integrated testing** — Built-in test framework
- **Readable code** — Cleaner than C, without sacrificing performance

## License

MIT

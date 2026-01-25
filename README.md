<p align="center"><br><br><br>
  <a href="https://octopass.linyo.ws">
    <img alt="OCTOPASS" src="https://github.com/linyows/octopass/blob/main/misc/octopass-logo-plain-2021.svg?raw=true" width="500">
  </a><br><br><br>
</p>

**Manage Linux users with your GitHub Organization/Team**

octopass brings GitHub's team management to your Linux servers. No more manually managing `/etc/passwd` or distributing SSH keys — just add users to your GitHub team, and they're ready to SSH into your servers.

<br>
<p align="center">
  <a href="https://github.com/linyows/octopass/actions/workflows/build.yml" title="Build"><img alt="GitHub Workflow Status" src="https://img.shields.io/github/actions/workflow/status/linyows/octopass/test.yml?branch=main&style=for-the-badge"></a>
  <a href="https://github.com/linyows/octopass/releases" title="GitHub release"><img src="http://img.shields.io/github/release/linyows/octopass.svg?style=for-the-badge"></a>
</p>

## Why octopass?

🔑 **SSH keys from GitHub** — Users authenticate with their GitHub SSH keys. No key distribution needed.

👥 **Team-based access** — Grant server access by GitHub team membership. Add to team = server access.

🔄 **Always in sync** — User lists and keys are fetched from GitHub API. Remove from team = access revoked.

🛡️ **Secure by design** — No passwords stored on servers. Authentication via GitHub personal access tokens.

📦 **Zero dependencies** — Single static binary. No runtime dependencies beyond libc.

## How it works

```
┌─────────────┐      ┌─────────────┐      ┌─────────────┐
│   GitHub    │      │   Server    │      │    User     │
│ Organization│◄────►│  (octopass) │◄────►│  SSH Client │
└─────────────┘      └─────────────┘      └─────────────┘
       │                    │                    │
       │  Team members      │  NSS module        │  SSH key
       │  SSH keys          │  provides user     │  authentication
       │  API responses     │  information       │
       └────────────────────┴────────────────────┘
```

octopass works as a **NSS (Name Service Switch) module**, seamlessly integrating GitHub teams into Linux user management:

- `getpwnam()` / `getpwuid()` → Returns GitHub team members as Linux users
- `getgrnam()` / `getgrgid()` → Returns GitHub team as a Linux group
- SSH `AuthorizedKeysCommand` → Fetches user's SSH public keys from GitHub

## Quick Start

### 1. Build

```bash
# Requires Zig 0.15+
zig build -Doptimize=ReleaseSafe
```

### 2. Install

```bash
# Install the NSS library
sudo cp zig-out/lib/libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/
sudo ln -sf libnss_octopass.so.2.0.0 /usr/lib/x86_64-linux-gnu/libnss_octopass.so.2

# Install the CLI
sudo cp zig-out/bin/octopass /usr/bin/
```

### 3. Configure

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

### 4. Enable NSS module

Edit `/etc/nsswitch.conf`:

```
passwd: files octopass
group:  files octopass
shadow: files octopass
```

### 5. Configure SSH

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

## Building from Source

```bash
# Debug build
zig build

# Release build
zig build -Doptimize=ReleaseSafe

# Run tests
zig build test

# Cross-compile for Linux (from macOS)
zig build -Dtarget=x86_64-linux-gnu -Doptimize=ReleaseSafe
```

## Architecture

```
src/
├── main.zig          # CLI entry point
├── config.zig        # Configuration parser
├── github.zig        # GitHub API client
├── cache.zig         # Response caching
├── types.zig         # Shared type definitions
├── log.zig           # Syslog wrapper
├── nss_exports.zig   # NSS C ABI exports
├── nss_passwd.zig    # passwd database
├── nss_group.zig     # group database
├── nss_shadow.zig    # shadow database
├── nss_common.zig    # NSS shared utilities
└── assets/
    ├── logo.txt
    ├── desc.txt
    └── usage.txt
```

## Why Zig?

This is a Zig rewrite of the original C implementation. Benefits:

- **Memory safety** — Compile-time checks prevent common vulnerabilities
- **No dependencies** — Zig's stdlib replaces libcurl and jansson
- **Easy cross-compilation** — Build for any target from any host
- **Integrated testing** — Built-in test framework
- **Readable code** — Cleaner than C, without sacrificing performance

## License

MIT

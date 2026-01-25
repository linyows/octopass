# SELinux Policy for octopass

## Why is this policy needed?

octopass is an NSS (Name Service Switch) module that authenticates Linux users via GitHub API. When running on systems with SELinux enabled (RHEL, CentOS, Fedora, Rocky Linux, etc.), the module executes within the security context of system processes like `sshd`, `chkpwd`, and `oddjob_mkhomedir`.

By default, SELinux restricts these processes from:

1. **Making outbound HTTPS connections** - octopass needs to call GitHub API
2. **Reading/writing cache files** - octopass caches API responses in `/var/cache/octopass`
3. **Managing SSL certificates** - Required for HTTPS communication

Without this policy, you will see AVC (Access Vector Cache) denied errors in `/var/log/audit/audit.log`:

```
type=AVC msg=audit(...): avc:  denied  { name_connect } for  pid=... comm="sshd" dest=443 scontext=system_u:system_r:sshd_t:s0 tcontext=system_u:object_r:http_port_t:s0 tclass=tcp_socket
```

## What permissions does this policy grant?

### For `sshd_t` (SSH daemon)
- Connect to HTTP/HTTPS ports (443) for GitHub API calls
- Create, read, and write files in `/var/cache/octopass`
- Manage SSL certificate files

### For `chkpwd_t` (Password checking helper)
- Connect to HTTP/HTTPS ports for authentication verification
- Read cache files

### For `oddjob_mkhomedir_t` (Home directory creation)
- Read cache files for user information during home directory creation

## Building the policy

```bash
# Install SELinux development tools
sudo dnf install selinux-policy-devel  # RHEL/CentOS/Fedora

# Build the policy module
make -f /usr/share/selinux/devel/Makefile
```

This generates `octopass.pp` (compiled policy package).

## Installing the policy manually

```bash
# Install the policy module
sudo semodule -i octopass.pp

# Verify installation
sudo semodule -l | grep octopass

# Restore file contexts
sudo restorecon -Rv /var/cache/octopass
sudo restorecon -Rv /usr/bin/octopass
```

## Removing the policy

```bash
sudo semodule -r octopass
```

## Troubleshooting

If octopass fails with SELinux enabled:

1. Check audit logs:
   ```bash
   sudo ausearch -m avc -ts recent | grep octopass
   ```

2. Generate policy suggestions:
   ```bash
   sudo ausearch -m avc -ts recent | audit2allow -M octopass_fix
   ```

3. Temporarily set SELinux to permissive mode for testing:
   ```bash
   sudo setenforce 0  # Permissive (logs but doesn't block)
   sudo setenforce 1  # Re-enable enforcing
   ```

## Note

The RPM package automatically installs and removes this policy during package installation/removal. Manual installation is only needed for development or custom deployments.

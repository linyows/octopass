# Packaging

This directory contains the configuration and tools for building distribution packages (deb/rpm) for octopass.

## Prerequisites

1. Docker and Docker Compose installed
2. Pre-built binaries in `zig-out/` (run `zig build cross` from the project root)

## Directory Structure

```
packaging/
├── Makefile.container    # Makefile for use inside containers (do not run on host)
├── docker-compose.yml    # Docker Compose configuration
├── docker/
│   ├── Dockerfile.deb    # Base image for Debian/Ubuntu builds
│   └── Dockerfile.rpm    # Base image for RHEL-based builds
├── debian/               # Debian packaging configuration
├── rpm/                  # RPM packaging configuration
├── selinux/              # SELinux policy (for RHEL-based systems)
└── dist/                 # Output directory for built packages
```

## Supported Distributions

| Distribution | Version | Package Type |
|--------------|---------|--------------|
| Ubuntu Noble | 24.04 LTS | deb |
| Ubuntu Questing | 25.04 | deb |
| Debian Bookworm | 12 (stable) | deb |
| Debian Trixie | 13 (testing) | deb |
| Rocky Linux | 9.x | rpm |
| Rocky Linux | 8.x | rpm |
| AlmaLinux | 9.x | rpm |
| AlmaLinux | 8.x | rpm |

## Building Packages

### Build all packages

```bash
cd packaging
docker compose up
```

### Build specific distribution

```bash
cd packaging

# Debian/Ubuntu
docker compose run --rm ubuntu-noble
docker compose run --rm ubuntu-questing
docker compose run --rm debian-bookworm
docker compose run --rm debian-trixie

# RHEL-based
docker compose run --rm rockylinux-9
docker compose run --rm rockylinux-8
docker compose run --rm almalinux-9
docker compose run --rm almalinux-8
```

### Rebuild images (after changing Dockerfile)

```bash
docker compose build --no-cache
```

## Output

Built packages are placed in `packaging/dist/`:

```
dist/
├── octopass_0.10.0-1_arm64.ubuntu-noble.deb
├── octopass_0.10.0-1_arm64.debian-bookworm.deb
├── octopass-0.10.0-1.el9.aarch64.rocky-9.3.rpm
└── ...
```

## Integration Testing

After building packages, integration tests are automatically executed to verify the package works correctly.

### Test Configuration

Create `packaging/octopass.conf` with valid GitHub credentials:

```bash
cp packaging/octopass.conf.example packaging/octopass.conf
# Edit octopass.conf with your GitHub token and organization/team settings
```

**Note:** `packaging/octopass.conf` contains sensitive credentials and should not be committed to git.

### What Tests Verify

1. **CLI commands**: version, help
2. **NSS library symbols**: All required NSS functions are exported
3. **API integration**:
   - `passwd` command returns valid passwd entries
   - `group` command returns valid group entries
   - `shadow` command returns valid shadow entries
   - SSH public keys retrieval works

### Running Tests Only

If packages are already built, you can run tests separately:

```bash
cd packaging

# Test DEB package
docker compose run --rm ubuntu-noble sh -c "make -f Makefile.container test_deb"

# Test RPM package
docker compose run --rm rockylinux-9 sh -c "make -f Makefile.container test_rpm"
```

## How It Works

1. Docker images are built with:
   - Required build tools (dpkg-dev, rpm-build, etc.)
   - Pre-built binaries copied from `zig-out/`
   - `octopass.conf.example` copied to image

2. The `packaging/` directory is mounted as a volume at `/packaging` inside the container

3. `Makefile.container` runs inside the container to:
   - Detect architecture and distribution
   - Create temporary build directory
   - Run dpkg-buildpackage or rpmbuild
   - Copy output to `/packaging/dist/`
   - Install the package and run integration tests

## Notes

- `Makefile.container` is designed to run **inside containers only**. Do not run it on the host.
- Packages are built for the host's CPU architecture (aarch64 or x86_64)
- SELinux policy is included in RPM packages (see `selinux/README.md`)
- Integration tests require `packaging/octopass.conf` with valid credentials

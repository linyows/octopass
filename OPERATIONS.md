Operations
==

New Release
--

### 1. Update changelogs

Update version and changelog entries:

- `packaging/rpm/octopass.spec`
- `packaging/debian/changelog`

### 2. Create and push tag

```sh
$ git tag v1.x.x
$ git push origin v1.x.x
```

### 3. Automated release process

The GitHub Actions release workflow will automatically:

1. Update `build.zig.zon` version from the git tag
2. Cross-compile binaries for Linux (amd64/arm64) and macOS (arm64)
3. Build DEB packages (Ubuntu Noble/Questing, Debian Bookworm/Trixie)
4. Build RPM packages (Rocky Linux 8/9, AlmaLinux 8/9)
5. Generate checksums and upload to GitHub Releases
6. Upload packages to PackageCloud

### Version Management

- **Development**: Version is fixed at `0.0.0-dev` in `build.zig.zon`
- **Release**: Version is injected from git tag (e.g., `v1.0.0` → `1.0.0`)
- **Binary**: Shows version as `octopass/x.x.x`

### Required Secrets

Configure the following secrets in GitHub repository settings:

- `PACKAGECLOUD_TOKEN`: Token for PackageCloud uploads

### Manual Local Build (for testing)

```sh
# Build for current platform
$ zig build

# Cross-compile for all platforms
$ zig build cross

# Run tests
$ zig build test
```

### Package Testing with Docker

```sh
$ cd packaging

# Build and test specific distribution
$ docker compose build ubuntu-noble
$ docker compose run --rm ubuntu-noble

# With environment variables for API testing
$ export OCTOPASS_TOKEN=xxx
$ export OCTOPASS_ORGANIZATION=xxx
$ export OCTOPASS_TEAM=xxx
$ docker compose run --rm ubuntu-noble
```

const std = @import("std");

const version: []const u8 = @import("build.zig.zon").version;

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Build options for version
    const options = b.addOptions();
    options.addOption([]const u8, "version", version);

    // NSS shared library
    const nss_module = b.createModule(.{
        .root_source_file = b.path("src/nss_exports.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    nss_module.addOptions("build_options", options);

    const nss_lib = b.addLibrary(.{
        .name = "nss_octopass",
        .root_module = nss_module,
        .linkage = .dynamic,
        .version = .{ .major = 2, .minor = 0, .patch = 0 },
    });

    // CLI executable
    const cli_module = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    cli_module.addOptions("build_options", options);

    const cli_exe = b.addExecutable(.{
        .name = "octopass",
        .root_module = cli_module,
    });

    // Install artifacts
    b.installArtifact(nss_lib);
    b.installArtifact(cli_exe);

    // Run step
    const run_cmd = b.addRunArtifact(cli_exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the octopass CLI");
    run_step.dependOn(&run_cmd.step);

    // Test step
    const types_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/types.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    const config_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/config.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    const cache_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/cache.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    const github_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/github.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    const gitlab_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/gitlab.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    const provider_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/provider.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    const run_types_tests = b.addRunArtifact(types_tests);
    const run_config_tests = b.addRunArtifact(config_tests);
    const run_cache_tests = b.addRunArtifact(cache_tests);
    const run_github_tests = b.addRunArtifact(github_tests);
    const run_gitlab_tests = b.addRunArtifact(gitlab_tests);
    const run_provider_tests = b.addRunArtifact(provider_tests);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_types_tests.step);
    test_step.dependOn(&run_config_tests.step);
    test_step.dependOn(&run_cache_tests.step);
    test_step.dependOn(&run_github_tests.step);
    test_step.dependOn(&run_gitlab_tests.step);
    test_step.dependOn(&run_provider_tests.step);

    // Format check
    const fmt_step = b.step("fmt", "Format source code");
    const fmt = b.addFmt(.{
        .paths = &.{
            "src",
            "build.zig",
        },
    });
    fmt_step.dependOn(&fmt.step);

    // Cross-compilation step
    const cross_step = b.step("cross", "Build for multiple targets (Linux amd64/arm64, macOS arm64)");

    const cross_targets = [_]struct {
        name: []const u8,
        target: std.Target.Query,
        build_nss: bool,
    }{
        .{ .name = "linux-amd64", .target = .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .gnu }, .build_nss = true },
        .{ .name = "linux-arm64", .target = .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .gnu }, .build_nss = true },
        .{ .name = "darwin-arm64", .target = .{ .cpu_arch = .aarch64, .os_tag = .macos }, .build_nss = false },
    };

    for (cross_targets) |ct| {
        const resolved_target = b.resolveTargetQuery(ct.target);

        // CLI executable
        const cross_cli_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = resolved_target,
            .optimize = .ReleaseSafe,
            .link_libc = true,
        });
        cross_cli_module.addOptions("build_options", options);

        const cross_cli = b.addExecutable(.{
            .name = "octopass",
            .root_module = cross_cli_module,
        });

        const cli_install = b.addInstallArtifact(cross_cli, .{
            .dest_dir = .{ .override = .{ .custom = ct.name } },
        });
        cross_step.dependOn(&cli_install.step);

        // NSS shared library (Linux only)
        if (ct.build_nss) {
            const cross_nss_module = b.createModule(.{
                .root_source_file = b.path("src/nss_exports.zig"),
                .target = resolved_target,
                .optimize = .ReleaseSafe,
                .link_libc = true,
            });
            cross_nss_module.addOptions("build_options", options);

            const cross_nss = b.addLibrary(.{
                .name = "nss_octopass",
                .root_module = cross_nss_module,
                .linkage = .dynamic,
                .version = .{ .major = 2, .minor = 0, .patch = 0 },
            });

            const nss_install = b.addInstallArtifact(cross_nss, .{
                .dest_dir = .{ .override = .{ .custom = ct.name } },
            });
            cross_step.dependOn(&nss_install.step);
        }
    }
}

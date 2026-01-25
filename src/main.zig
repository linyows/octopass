const std = @import("std");
const config_mod = @import("config.zig");
const types = @import("types.zig");
const log = @import("log.zig");
const github_mod = @import("github.zig");
const nss_common = @import("nss_common.zig");

const Allocator = std.mem.Allocator;

// ANSI color codes
const ANSI_GREEN = "\x1b[32m";
const ANSI_DIM = "\x1b[2m";
const ANSI_RED = "\x1b[31m";
const ANSI_RESET = "\x1b[0m";

// Embedded assets
const logo = @embedFile("assets/logo.txt");
const desc = @embedFile("assets/desc.txt");
const usage = @embedFile("assets/usage.txt");

const Command = enum {
    passwd,
    shadow,
    group,
    pam,
    version,
    help,
    keys, // Default: get public keys for a user
};

/// Write formatted output to stdout
fn writeStdout(comptime fmt: []const u8, args: anytype) void {
    var buf: [4096]u8 = undefined;
    const result = std.fmt.bufPrint(&buf, fmt, args) catch return;
    _ = std.posix.write(std.posix.STDOUT_FILENO, result) catch return;
}

/// Write error message to stderr
fn writeError(comptime fmt: []const u8, args: anytype) void {
    var buf: [4096]u8 = undefined;
    const result = std.fmt.bufPrint(&buf, ANSI_RED ++ "Error: " ++ ANSI_RESET ++ fmt, args) catch return;
    _ = std.posix.write(std.posix.STDERR_FILENO, result) catch return;
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    // Parse global options first
    var config_path: []const u8 = types.default_config_file;
    var config_path_buf: [std.fs.max_path_bytes]u8 = undefined;
    var arg_index: usize = 1;

    while (arg_index < args.len) {
        const arg = args[arg_index];
        if (std.mem.eql(u8, arg, "-c") or std.mem.eql(u8, arg, "--config")) {
            if (arg_index + 1 >= args.len) {
                writeError("Missing argument for {s}\n", .{arg});
                std.process.exit(2);
            }
            const path_arg = args[arg_index + 1];
            // Convert relative path to absolute path
            if (!std.fs.path.isAbsolute(path_arg)) {
                const cwd = std.fs.cwd();
                config_path = cwd.realpath(path_arg, &config_path_buf) catch |err| {
                    writeError("Failed to resolve config path '{s}': {}\n", .{ path_arg, err });
                    std.process.exit(2);
                };
            } else {
                config_path = path_arg;
            }
            arg_index += 2;
        } else {
            break;
        }
    }

    if (arg_index >= args.len) {
        printUsage();
        std.process.exit(2);
    }

    const arg1 = args[arg_index];

    // Check for help/version flags
    if (std.mem.eql(u8, arg1, "--help") or std.mem.eql(u8, arg1, "-h")) {
        printUsage();
        std.process.exit(2);
    }

    if (std.mem.eql(u8, arg1, "--version") or std.mem.eql(u8, arg1, "-v")) {
        printVersion();
        return;
    }

    // Parse command
    if (std.mem.eql(u8, arg1, "passwd")) {
        const key = if (arg_index + 1 < args.len) args[arg_index + 1] else null;
        try runPasswd(allocator, config_path, key);
    } else if (std.mem.eql(u8, arg1, "group")) {
        const key = if (arg_index + 1 < args.len) args[arg_index + 1] else null;
        try runGroup(allocator, config_path, key);
    } else if (std.mem.eql(u8, arg1, "shadow")) {
        const key = if (arg_index + 1 < args.len) args[arg_index + 1] else null;
        try runShadow(allocator, config_path, key);
    } else if (std.mem.eql(u8, arg1, "pam")) {
        const user = if (arg_index + 1 < args.len) args[arg_index + 1] else std.posix.getenv("PAM_USER");
        if (user == null) {
            writeError("User is required\n", .{});
            std.process.exit(2);
        }
        const exit_code = try runPam(allocator, config_path, user.?);
        std.process.exit(exit_code);
    } else {
        // Default: treat as username and get public keys
        try runKeys(allocator, config_path, arg1);
    }
}

fn printVersion() void {
    writeStdout("{s}\n", .{types.version_with_name});
}

fn printUsage() void {
    // Logo with green color
    _ = std.posix.write(std.posix.STDOUT_FILENO, ANSI_GREEN) catch {};
    _ = std.posix.write(std.posix.STDOUT_FILENO, logo) catch {};
    _ = std.posix.write(std.posix.STDOUT_FILENO, ANSI_RESET) catch {};

    // Description with dim color
    _ = std.posix.write(std.posix.STDOUT_FILENO, "\n") catch {};
    _ = std.posix.write(std.posix.STDOUT_FILENO, ANSI_DIM) catch {};
    _ = std.posix.write(std.posix.STDOUT_FILENO, desc) catch {};
    _ = std.posix.write(std.posix.STDOUT_FILENO, ANSI_RESET) catch {};

    // Usage
    _ = std.posix.write(std.posix.STDOUT_FILENO, "\n") catch {};
    _ = std.posix.write(std.posix.STDOUT_FILENO, usage) catch {};
}

fn loadConfig(allocator: Allocator, config_path: []const u8) !config_mod.Config {
    return config_mod.Config.load(allocator, config_path) catch |err| {
        writeError("Failed to load config: {}\n", .{err});
        return err;
    };
}

/// Get public keys for a user (default command)
fn runKeys(allocator: Allocator, config_path: []const u8, username: []const u8) !void {
    var config = try loadConfig(allocator, config_path);
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var github = github_mod.GitHubProvider.init(allocator, &config, &logger);
    defer github.deinit();

    // Check if user is a shared user
    for (config.shared_users) |shared_user| {
        if (std.mem.eql(u8, username, shared_user)) {
            // Get all team members' keys for shared users
            const users = github.getMembers(allocator) catch |err| {
                writeError("Failed to get members: {}\n", .{err});
                std.process.exit(1);
            };
            defer types.freeUsers(allocator, users);

            for (users) |user| {
                const keys = github.getUserKeys(allocator, user.login) catch continue;
                defer allocator.free(keys);
                _ = std.posix.write(std.posix.STDOUT_FILENO, keys) catch {};
            }
            return;
        }
    }

    // Get keys for specific user
    const keys = github.getUserKeys(allocator, username) catch |err| {
        writeError("Failed to get keys for {s}: {}\n", .{ username, err });
        std.process.exit(1);
    };
    defer allocator.free(keys);

    _ = std.posix.write(std.posix.STDOUT_FILENO, keys) catch return;
}

/// PAM authentication - read token from stdin
fn runPam(allocator: Allocator, config_path: []const u8, username: []const u8) !u8 {
    // Read token from stdin
    const stdin = std.fs.File{ .handle = std.posix.STDIN_FILENO };
    var buf: [4096]u8 = undefined;

    var token_buf = std.ArrayListUnmanaged(u8){};
    defer token_buf.deinit(allocator);

    while (true) {
        const n = stdin.read(&buf) catch {
            writeError("Failed to read token from stdin\n", .{});
            return 2;
        };
        if (n == 0) break;
        token_buf.appendSlice(allocator, buf[0..n]) catch {
            writeError("Out of memory\n", .{});
            return 2;
        };
    }

    // Trim newline
    var token = token_buf.items;
    if (token.len > 0 and token[token.len - 1] == '\n') {
        token = token[0 .. token.len - 1];
    }

    if (token.len == 0) {
        writeError("Token is required\n", .{});
        return 2;
    }

    var config = loadConfig(allocator, config_path) catch return 2;
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var github = github_mod.GitHubProvider.init(allocator, &config, &logger);
    defer github.deinit();

    // Authenticate with token
    const authenticated = github.authenticate(username, token) catch {
        return 1;
    };

    if (authenticated) {
        return 0;
    } else {
        return 1;
    }
}

/// Display passwd entries
fn runPasswd(allocator: Allocator, config_path: []const u8, key: ?[]const u8) !void {
    var config = try loadConfig(allocator, config_path);
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var github = github_mod.GitHubProvider.init(allocator, &config, &logger);
    defer github.deinit();

    const users = github.getMembers(allocator) catch |err| {
        writeError("Failed to get members: {}\n", .{err});
        std.process.exit(1);
    };
    defer types.freeUsers(allocator, users);

    if (key) |k| {
        // Check if key is a number (UID) or name
        const uid_opt = std.fmt.parseInt(i64, k, 10) catch null;

        if (uid_opt) |uid| {
            // Find by UID
            for (users) |user| {
                const user_uid = config.uid_starts + user.id;
                if (user_uid == uid) {
                    printPasswdEntry(&config, user);
                    return;
                }
            }
        } else {
            // Find by name
            if (types.findUserByLogin(users, k)) |user| {
                printPasswdEntry(&config, user.*);
                return;
            }
        }
    } else {
        // List all
        for (users) |user| {
            printPasswdEntry(&config, user);
        }
    }
}

fn printPasswdEntry(config: *const config_mod.Config, user: types.User) void {
    const uid = config.uid_starts + user.id;

    var home_buf: [256]u8 = undefined;
    const home = if (nss_common.simpleFormatHomePath(&home_buf, config.home, user.login)) |h| h else "/home/user";

    writeStdout("{s}:x:{d}:{d}:managed by octopass:{s}:{s}\n", .{
        user.login,
        uid,
        config.gid,
        home,
        config.shell,
    });
}

/// Display group entries
fn runGroup(allocator: Allocator, config_path: []const u8, key: ?[]const u8) !void {
    var config = try loadConfig(allocator, config_path);
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var github = github_mod.GitHubProvider.init(allocator, &config, &logger);
    defer github.deinit();

    const users = github.getMembers(allocator) catch |err| {
        writeError("Failed to get members: {}\n", .{err});
        std.process.exit(1);
    };
    defer types.freeUsers(allocator, users);

    const group_name = config.group_name orelse "octopass";

    if (key) |k| {
        // Check if key is a number (GID) or name
        const gid_opt = std.fmt.parseInt(i64, k, 10) catch null;

        if (gid_opt) |gid| {
            if (gid != config.gid) return;
        } else {
            if (!std.mem.eql(u8, k, group_name)) return;
        }
    }

    // Print group entry
    writeStdout("{s}:x:{d}:", .{ group_name, config.gid });

    // Print members
    for (users, 0..) |user, i| {
        if (i > 0) writeStdout(",", .{});
        writeStdout("{s}", .{user.login});
    }
    writeStdout("\n", .{});
}

/// Display shadow entries
fn runShadow(allocator: Allocator, config_path: []const u8, key: ?[]const u8) !void {
    var config = try loadConfig(allocator, config_path);
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var github = github_mod.GitHubProvider.init(allocator, &config, &logger);
    defer github.deinit();

    const users = github.getMembers(allocator) catch |err| {
        writeError("Failed to get members: {}\n", .{err});
        std.process.exit(1);
    };
    defer types.freeUsers(allocator, users);

    if (key) |k| {
        // Check if key is a number (invalid for shadow) or name
        const is_number = std.fmt.parseInt(i64, k, 10) catch null;

        if (is_number != null) {
            writeError("Invalid arguments: {s}\n", .{k});
            return;
        }

        // Find by name
        if (types.findUserByLogin(users, k)) |user| {
            printShadowEntry(user.*);
            return;
        }
    } else {
        // List all
        for (users) |user| {
            printShadowEntry(user);
        }
    }
}

fn printShadowEntry(user: types.User) void {
    // Format: name:password:lastchg:min:max:warn:inactive:expire:reserved
    // Using !! for locked password and -1 for unset values
    writeStdout("{s}:!!:-1:-1:-1:-1:-1:-1:\n", .{user.login});
}

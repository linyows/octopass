const std = @import("std");
const config_mod = @import("config.zig");
const types = @import("types.zig");
const log = @import("log.zig");
const provider_mod = @import("provider.zig");
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

/// Write raw bytes to stdout
fn writeOut(io: std.Io, bytes: []const u8) void {
    std.Io.File.stdout().writeStreamingAll(io, bytes) catch return;
}

/// Write formatted output to stdout
fn writeStdout(io: std.Io, comptime fmt: []const u8, args: anytype) void {
    var buf: [4096]u8 = undefined;
    const result = std.fmt.bufPrint(&buf, fmt, args) catch return;
    writeOut(io, result);
}

/// Write error message to stderr
fn writeError(io: std.Io, comptime fmt: []const u8, args: anytype) void {
    var buf: [4096]u8 = undefined;
    const result = std.fmt.bufPrint(&buf, ANSI_RED ++ "Error: " ++ ANSI_RESET ++ fmt, args) catch return;
    std.Io.File.stderr().writeStreamingAll(io, result) catch return;
}

pub fn main(init: std.process.Init) !void {
    const allocator = init.gpa;
    const io = init.io;

    const args = try init.minimal.args.toSlice(init.arena.allocator());

    // Parse global options first
    var config_path: []const u8 = types.default_config_file;
    var config_path_buf: [std.fs.max_path_bytes]u8 = undefined;
    var arg_index: usize = 1;

    while (arg_index < args.len) {
        const arg = args[arg_index];
        if (std.mem.eql(u8, arg, "-c") or std.mem.eql(u8, arg, "--config")) {
            if (arg_index + 1 >= args.len) {
                writeError(io, "Missing argument for {s}\n", .{arg});
                std.process.exit(2);
            }
            const path_arg = args[arg_index + 1];
            // Convert relative path to absolute path
            if (!std.fs.path.isAbsolute(path_arg)) {
                const len = std.Io.Dir.cwd().realPathFile(io, path_arg, &config_path_buf) catch |err| {
                    writeError(io, "Failed to resolve config path '{s}': {}\n", .{ path_arg, err });
                    std.process.exit(2);
                };
                config_path = config_path_buf[0..len];
            } else {
                config_path = path_arg;
            }
            arg_index += 2;
        } else {
            break;
        }
    }

    if (arg_index >= args.len) {
        printUsage(io);
        std.process.exit(2);
    }

    const arg1 = args[arg_index];

    // Check for help/version flags
    if (std.mem.eql(u8, arg1, "--help") or std.mem.eql(u8, arg1, "-h")) {
        printUsage(io);
        std.process.exit(2);
    }

    if (std.mem.eql(u8, arg1, "--version") or std.mem.eql(u8, arg1, "-v")) {
        printVersion(io);
        return;
    }

    // Parse command
    if (std.mem.eql(u8, arg1, "passwd")) {
        const key = if (arg_index + 1 < args.len) args[arg_index + 1] else null;
        try runPasswd(allocator, io, config_path, key);
    } else if (std.mem.eql(u8, arg1, "group")) {
        const key = if (arg_index + 1 < args.len) args[arg_index + 1] else null;
        try runGroup(allocator, io, config_path, key);
    } else if (std.mem.eql(u8, arg1, "shadow")) {
        const key = if (arg_index + 1 < args.len) args[arg_index + 1] else null;
        try runShadow(allocator, io, config_path, key);
    } else if (std.mem.eql(u8, arg1, "pam")) {
        const user = if (arg_index + 1 < args.len) args[arg_index + 1] else init.environ_map.get("PAM_USER");
        if (user == null) {
            writeError(io, "User is required\n", .{});
            std.process.exit(2);
        }
        const exit_code = try runPam(allocator, io, config_path, user.?);
        std.process.exit(exit_code);
    } else {
        // Default: treat as username and get public keys
        try runKeys(allocator, io, config_path, arg1);
    }
}

fn printVersion(io: std.Io) void {
    writeStdout(io, "{s}\n", .{types.version_with_name});
}

fn printUsage(io: std.Io) void {
    // Logo with green color
    writeOut(io, ANSI_GREEN);
    writeOut(io, logo);
    writeOut(io, ANSI_RESET);

    // Description with dim color
    writeOut(io, "\n");
    writeOut(io, ANSI_DIM);
    writeOut(io, desc);
    writeOut(io, ANSI_RESET);

    // Usage
    writeOut(io, "\n");
    writeOut(io, usage);
}

fn loadConfig(allocator: Allocator, io: std.Io, config_path: []const u8) !config_mod.Config {
    return config_mod.Config.load(allocator, io, config_path) catch |err| {
        writeError(io, "Failed to load config: {}\n", .{err});
        return err;
    };
}

/// Get public keys for a user (default command)
fn runKeys(allocator: Allocator, io: std.Io, config_path: []const u8, username: []const u8) !void {
    var config = try loadConfig(allocator, io, config_path);
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var provider = provider_mod.Provider.init(allocator, io, &config, &logger);
    defer provider.deinit();

    // Check if user is a shared user
    for (config.shared_users) |shared_user| {
        if (std.mem.eql(u8, username, shared_user)) {
            // Get all team members' keys for shared users
            const users = provider.getMembers(allocator) catch |err| {
                writeError(io, "Failed to get members: {}\n", .{err});
                std.process.exit(1);
            };
            defer types.freeUsers(allocator, users);

            for (users) |user| {
                const keys = provider.getUserKeys(allocator, user.login) catch continue;
                defer allocator.free(keys);
                writeOut(io, keys);
            }
            return;
        }
    }

    // Get keys for specific user
    const keys = provider.getUserKeys(allocator, username) catch |err| {
        writeError(io, "Failed to get keys for {s}: {}\n", .{ username, err });
        std.process.exit(1);
    };
    defer allocator.free(keys);

    writeOut(io, keys);
}

/// PAM authentication - read token from stdin
fn runPam(allocator: Allocator, io: std.Io, config_path: []const u8, username: []const u8) !u8 {
    // Read token from stdin
    var buf: [4096]u8 = undefined;
    var stdin_reader = std.Io.File.stdin().readerStreaming(io, &buf);

    const token_buf = stdin_reader.interface.allocRemaining(
        allocator,
        .limited(types.max_buffer_size),
    ) catch {
        writeError(io, "Failed to read token from stdin\n", .{});
        return 2;
    };
    defer allocator.free(token_buf);

    // Trim newline
    var token = token_buf;
    if (token.len > 0 and token[token.len - 1] == '\n') {
        token = token[0 .. token.len - 1];
    }

    if (token.len == 0) {
        writeError(io, "Token is required\n", .{});
        return 2;
    }

    var config = loadConfig(allocator, io, config_path) catch return 2;
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var provider = provider_mod.Provider.init(allocator, io, &config, &logger);
    defer provider.deinit();

    // Authenticate with token
    const authenticated = provider.authenticate(username, token) catch {
        return 1;
    };

    if (authenticated) {
        return 0;
    } else {
        return 1;
    }
}

/// Display passwd entries
fn runPasswd(allocator: Allocator, io: std.Io, config_path: []const u8, key: ?[]const u8) !void {
    var config = try loadConfig(allocator, io, config_path);
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var provider = provider_mod.Provider.init(allocator, io, &config, &logger);
    defer provider.deinit();

    const users = provider.getMembers(allocator) catch |err| {
        writeError(io, "Failed to get members: {}\n", .{err});
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
                    printPasswdEntry(io, &config, user);
                    return;
                }
            }
        } else {
            // Find by name
            if (types.findUserByLogin(users, k)) |user| {
                printPasswdEntry(io, &config, user.*);
                return;
            }
        }
    } else {
        // List all
        for (users) |user| {
            printPasswdEntry(io, &config, user);
        }
    }
}

fn printPasswdEntry(io: std.Io, config: *const config_mod.Config, user: types.User) void {
    const uid = config.uid_starts + user.id;

    var home_buf: [256]u8 = undefined;
    const home = if (nss_common.simpleFormatHomePath(&home_buf, config.home, user.login)) |h| h else "/home/user";

    writeStdout(io, "{s}:x:{d}:{d}:managed by octopass:{s}:{s}\n", .{
        user.login,
        uid,
        config.gid,
        home,
        config.shell,
    });
}

/// Display group entries
fn runGroup(allocator: Allocator, io: std.Io, config_path: []const u8, key: ?[]const u8) !void {
    var config = try loadConfig(allocator, io, config_path);
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var provider = provider_mod.Provider.init(allocator, io, &config, &logger);
    defer provider.deinit();

    const users = provider.getMembers(allocator) catch |err| {
        writeError(io, "Failed to get members: {}\n", .{err});
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
    writeStdout(io, "{s}:x:{d}:", .{ group_name, config.gid });

    // Print members
    for (users, 0..) |user, i| {
        if (i > 0) writeStdout(io, ",", .{});
        writeStdout(io, "{s}", .{user.login});
    }
    writeStdout(io, "\n", .{});
}

/// Display shadow entries
fn runShadow(allocator: Allocator, io: std.Io, config_path: []const u8, key: ?[]const u8) !void {
    var config = try loadConfig(allocator, io, config_path);
    defer config.deinit();

    var logger = log.Logger.init("octopass", config.syslog);
    defer logger.close();

    var provider = provider_mod.Provider.init(allocator, io, &config, &logger);
    defer provider.deinit();

    const users = provider.getMembers(allocator) catch |err| {
        writeError(io, "Failed to get members: {}\n", .{err});
        std.process.exit(1);
    };
    defer types.freeUsers(allocator, users);

    if (key) |k| {
        // Check if key is a number (invalid for shadow) or name
        const is_number = std.fmt.parseInt(i64, k, 10) catch null;

        if (is_number != null) {
            writeError(io, "Invalid arguments: {s}\n", .{k});
            return;
        }

        // Find by name
        if (types.findUserByLogin(users, k)) |user| {
            printShadowEntry(io, user.*);
            return;
        }
    } else {
        // List all
        for (users) |user| {
            printShadowEntry(io, user);
        }
    }
}

fn printShadowEntry(io: std.Io, user: types.User) void {
    // Format: name:password:lastchg:min:max:warn:inactive:expire:reserved
    // Using !! for locked password and -1 for unset values
    writeStdout(io, "{s}:!!:-1:-1:-1:-1:-1:-1:\n", .{user.login});
}

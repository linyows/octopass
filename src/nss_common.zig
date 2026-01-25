const std = @import("std");
const Allocator = std.mem.Allocator;

const config_mod = @import("config.zig");
const types = @import("types.zig");
const log = @import("log.zig");
const github_mod = @import("github.zig");

pub const NssStatus = types.NssStatus;

/// Global state for NSS enumeration
pub const GlobalState = struct {
    config: ?config_mod.Config = null,
    users: ?[]types.User = null,
    index: usize = 0,
    initialized: bool = false,
    allocator: Allocator,
    logger: log.Logger,

    const Self = @This();

    pub fn init(allocator: Allocator) Self {
        return .{
            .allocator = allocator,
            .logger = log.Logger.init("octopass", false),
        };
    }

    pub fn deinit(self: *Self) void {
        self.reset();
        self.logger.close();
    }

    pub fn reset(self: *Self) void {
        if (self.users) |users| {
            types.freeUsers(self.allocator, users);
            self.users = null;
        }
        if (self.config) |*conf| {
            conf.deinit();
            self.config = null;
        }
        self.index = 0;
        self.initialized = false;
    }

    pub fn loadConfig(self: *Self) !void {
        if (self.config != null) return;

        self.config = config_mod.Config.load(self.allocator, types.default_config_file) catch |err| {
            self.logger.err("config load failed: {}", .{err});
            return error.ConfigNotFound;
        };

        self.logger = log.Logger.init("octopass", self.config.?.syslog);
        if (self.config.?.syslog) {
            self.logger.open();
        }
    }

    pub fn loadMembers(self: *Self) !void {
        if (self.users != null) return;

        try self.loadConfig();

        var github = github_mod.GitHubProvider.init(self.allocator, &self.config.?, &self.logger);
        defer github.deinit();

        self.users = github.getMembers(self.allocator) catch |err| {
            self.logger.err("getMembers failed: {}", .{err});
            return error.NetworkError;
        };

        self.initialized = true;
    }
};

/// Thread-safe mutex for NSS operations
pub var nss_mutex: std.Thread.Mutex = .{};

/// Global allocator for NSS (using C allocator for compatibility)
pub const nss_allocator = std.heap.c_allocator;

/// Global state instance
var global_state: ?GlobalState = null;

pub fn getGlobalState() *GlobalState {
    if (global_state == null) {
        global_state = GlobalState.init(nss_allocator);
    }
    return &global_state.?;
}

/// Copy string to caller-provided buffer
pub fn copyToBuffer(dest: [*]u8, dest_len: usize, src: []const u8) ?[*:0]u8 {
    if (src.len >= dest_len) return null;

    @memcpy(dest[0..src.len], src);
    dest[src.len] = 0;

    return @ptrCast(dest);
}

/// Simple home path formatting (direct replacement of %s)
pub fn simpleFormatHomePath(buffer: []u8, template: []const u8, username: []const u8) ?[]const u8 {
    var write_pos: usize = 0;
    var read_pos: usize = 0;

    while (read_pos < template.len) {
        if (read_pos + 1 < template.len and template[read_pos] == '%' and template[read_pos + 1] == 's') {
            if (write_pos + username.len >= buffer.len) return null;
            @memcpy(buffer[write_pos..][0..username.len], username);
            write_pos += username.len;
            read_pos += 2;
        } else {
            if (write_pos >= buffer.len) return null;
            buffer[write_pos] = template[read_pos];
            write_pos += 1;
            read_pos += 1;
        }
    }

    if (write_pos >= buffer.len) return null;
    buffer[write_pos] = 0;

    return buffer[0..write_pos];
}

// Tests
test "copyToBuffer" {
    var buffer: [64]u8 = undefined;

    const result = copyToBuffer(&buffer, buffer.len, "hello");
    try std.testing.expect(result != null);

    const span = std.mem.span(result.?);
    try std.testing.expectEqualStrings("hello", span);
}

test "simpleFormatHomePath" {
    var buffer: [256]u8 = undefined;

    const result = simpleFormatHomePath(&buffer, "/home/%s", "alice");
    try std.testing.expect(result != null);
    try std.testing.expectEqualStrings("/home/alice", result.?);
}

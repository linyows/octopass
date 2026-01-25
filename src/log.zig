const std = @import("std");

// C syslog bindings
const c = @cImport({
    @cInclude("syslog.h");
});

pub const Level = enum(c_int) {
    emerg = 0,
    alert = 1,
    crit = 2,
    err = 3,
    warning = 4,
    notice = 5,
    info = 6,
    debug = 7,
};

pub const Facility = enum(c_int) {
    user = 1 << 3,
};

pub const Option = struct {
    pub const pid: c_int = 0x01;
    pub const cons: c_int = 0x02;
};

pub const Logger = struct {
    enabled: bool,
    ident: [:0]const u8,
    is_open: bool = false,

    const Self = @This();

    pub fn init(ident: [:0]const u8, enabled: bool) Self {
        return .{
            .enabled = enabled,
            .ident = ident,
        };
    }

    pub fn open(self: *Self) void {
        if (!self.enabled or self.is_open) return;

        c.openlog(self.ident.ptr, Option.cons | Option.pid, @intFromEnum(Facility.user));
        self.is_open = true;
    }

    pub fn close(self: *Self) void {
        if (!self.is_open) return;

        c.closelog();
        self.is_open = false;
    }

    pub fn log(self: *Self, level: Level, comptime fmt: []const u8, args: anytype) void {
        if (!self.enabled) return;

        if (!self.is_open) {
            self.open();
        }

        var buf: [4096]u8 = undefined;
        const message = std.fmt.bufPrint(&buf, fmt, args) catch return;

        // Ensure null-terminated
        if (message.len < buf.len) {
            buf[message.len] = 0;
            c.syslog(@intFromEnum(level), "%s", &buf);
        }
    }

    pub fn info(self: *Self, comptime fmt: []const u8, args: anytype) void {
        self.log(.info, fmt, args);
    }

    pub fn err(self: *Self, comptime fmt: []const u8, args: anytype) void {
        self.log(.err, fmt, args);
    }

    pub fn debug(self: *Self, comptime fmt: []const u8, args: anytype) void {
        self.log(.debug, fmt, args);
    }

    pub fn warning(self: *Self, comptime fmt: []const u8, args: anytype) void {
        self.log(.warning, fmt, args);
    }
};

/// Global logger instance
var global_logger: ?Logger = null;

pub fn getGlobalLogger() *Logger {
    if (global_logger == null) {
        global_logger = Logger.init("octopass", false);
    }
    return &global_logger.?;
}

pub fn initGlobalLogger(enabled: bool) void {
    global_logger = Logger.init("octopass", enabled);
    if (enabled) {
        global_logger.?.open();
    }
}

pub fn closeGlobalLogger() void {
    if (global_logger) |*logger| {
        logger.close();
    }
}

// Tests
test "Logger init" {
    const logger = Logger.init("test", false);
    try std.testing.expectEqual(false, logger.enabled);
    try std.testing.expectEqual(false, logger.is_open);
}

test "Logger disabled does not log" {
    var logger = Logger.init("test", false);
    // This should not crash or do anything
    logger.info("test message: {}", .{42});
    try std.testing.expectEqual(false, logger.is_open);
}

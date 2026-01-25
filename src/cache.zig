const std = @import("std");
const types = @import("types.zig");
const Allocator = std.mem.Allocator;

pub const CacheError = error{
    CreateDirectoryFailed,
    WriteFileFailed,
    ReadFileFailed,
    InvalidPath,
    OutOfMemory,
};

pub const fallback_cache_dir = "/tmp/octopass";

pub const Cache = struct {
    allocator: Allocator,
    cache_dir: []const u8,
    ttl_seconds: i64,
    enabled: bool,

    const Self = @This();

    pub fn init(allocator: Allocator, cache_dir: []const u8, ttl_seconds: i64) Self {
        // Check if cache_dir exists, fallback to /tmp/octopass if not
        const actual_cache_dir = blk: {
            std.fs.accessAbsolute(cache_dir, .{}) catch {
                break :blk fallback_cache_dir;
            };
            break :blk cache_dir;
        };

        return .{
            .allocator = allocator,
            .cache_dir = actual_cache_dir,
            .ttl_seconds = ttl_seconds,
            .enabled = ttl_seconds > 0,
        };
    }

    /// Generate cache key from URL and token prefix
    pub fn generateKey(self: *Self, url: []const u8, token_prefix: []const u8) ![]const u8 {
        // URL encode the URL for safe filename
        const encoded_url = try urlEncode(self.allocator, url);
        defer self.allocator.free(encoded_url);

        // Get effective uid for per-user cache directory
        const euid = std.posix.geteuid();

        // Build path: cache_dir/euid/encoded_url-token_prefix
        return std.fmt.allocPrint(self.allocator, "{s}/{d}/{s}-{s}", .{
            self.cache_dir,
            euid,
            encoded_url,
            token_prefix,
        });
    }

    /// Get cached data if valid
    pub fn get(self: *Self, key: []const u8) ?[]const u8 {
        if (!self.enabled) return null;

        const file = std.fs.openFileAbsolute(key, .{}) catch return null;
        defer file.close();

        // Check file modification time
        const stat = file.stat() catch return null;
        const mtime_sec = @divFloor(stat.mtime, std.time.ns_per_s);
        const now = std.time.timestamp();

        if (now - mtime_sec > self.ttl_seconds) {
            // Cache expired
            return null;
        }

        // Read file content
        const content = file.readToEndAlloc(self.allocator, types.max_buffer_size) catch return null;
        return content;
    }

    /// Store data in cache
    pub fn put(self: *Self, key: []const u8, data: []const u8) !void {
        if (!self.enabled) return;

        // Extract directory from key
        const dir_end = std.mem.lastIndexOf(u8, key, "/") orelse return error.InvalidPath;
        const dir_path = key[0..dir_end];

        // Create directory structure
        try ensureDirectory(dir_path);

        // Write file
        const file = std.fs.createFileAbsolute(key, .{}) catch return error.WriteFileFailed;
        defer file.close();

        file.writeAll(data) catch return error.WriteFileFailed;
    }

    /// Invalidate cache entry
    pub fn invalidate(self: *Self, key: []const u8) void {
        _ = self;
        std.fs.deleteFileAbsolute(key) catch {};
    }

    /// Clear all cache
    pub fn clear(self: *Self) void {
        std.fs.deleteTreeAbsolute(self.cache_dir) catch {};
    }
};

/// Ensure directory exists
fn ensureDirectory(path: []const u8) !void {
    std.fs.makeDirAbsolute(path) catch |err| {
        switch (err) {
            error.PathAlreadyExists => {},
            error.FileNotFound => {
                // Parent directory doesn't exist, create recursively
                if (std.mem.lastIndexOf(u8, path, "/")) |parent_end| {
                    if (parent_end > 0) {
                        try ensureDirectory(path[0..parent_end]);
                        std.fs.makeDirAbsolute(path) catch |e| {
                            if (e != error.PathAlreadyExists) return e;
                        };
                    }
                }
            },
            else => return err,
        }
    };
}

/// URL encode a string for safe filename
fn urlEncode(allocator: Allocator, input: []const u8) ![]const u8 {
    var result = std.ArrayListUnmanaged(u8){};
    errdefer result.deinit(allocator);

    for (input) |char| {
        if (std.ascii.isAlphanumeric(char) or char == '-' or char == '_' or char == '.') {
            try result.append(allocator, char);
        } else {
            // Percent-encode the character
            var buf: [3]u8 = undefined;
            const encoded = std.fmt.bufPrint(&buf, "%{X:0>2}", .{char}) catch continue;
            try result.appendSlice(allocator, encoded);
        }
    }

    return result.toOwnedSlice(allocator);
}

// Tests
test "urlEncode" {
    const allocator = std.testing.allocator;

    const encoded = try urlEncode(allocator, "https://api.github.com/test");
    defer allocator.free(encoded);

    try std.testing.expectEqualStrings("https%3A%2F%2Fapi.github.com%2Ftest", encoded);
}

test "urlEncode safe chars" {
    const allocator = std.testing.allocator;

    const encoded = try urlEncode(allocator, "test-file_name.json");
    defer allocator.free(encoded);

    try std.testing.expectEqualStrings("test-file_name.json", encoded);
}

test "Cache init" {
    const allocator = std.testing.allocator;
    const cache = Cache.init(allocator, "/tmp/cache", 300);

    try std.testing.expect(cache.enabled);
    try std.testing.expectEqual(@as(i64, 300), cache.ttl_seconds);
}

test "Cache disabled when ttl is 0" {
    const allocator = std.testing.allocator;
    const cache = Cache.init(allocator, "/tmp/cache", 0);

    try std.testing.expect(!cache.enabled);
}

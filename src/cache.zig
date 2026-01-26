const std = @import("std");
const types = @import("types.zig");
const Allocator = std.mem.Allocator;

pub const CacheError = error{
    CreateDirectoryFailed,
    WriteFileFailed,
    ReadFileFailed,
    InvalidPath,
    OutOfMemory,
    SignatureVerificationFailed,
};

pub const fallback_cache_dir = "/tmp/octopass";

pub const Cache = struct {
    allocator: Allocator,
    cache_dir: []const u8,
    ttl_seconds: i64,
    enabled: bool,
    token: []const u8,

    const Self = @This();
    const HmacSha256 = std.crypto.auth.hmac.sha2.HmacSha256;

    pub fn init(allocator: Allocator, cache_dir: []const u8, ttl_seconds: i64, token: []const u8) Self {
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
            .token = token,
        };
    }

    /// Generate cache key from URL and token prefix
    pub fn generateKey(self: *Self, url: []const u8, token_prefix: []const u8) ![]const u8 {
        // URL encode the URL for safe filename
        const encoded_url = try urlEncode(self.allocator, url);
        defer self.allocator.free(encoded_url);

        // Build path: cache_dir/encoded_url-token_prefix (shared across all users)
        return std.fmt.allocPrint(self.allocator, "{s}/{s}-{s}", .{
            self.cache_dir,
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

        // Verify signature: first 64 chars are hex signature, then newline, then data
        if (content.len < 66) { // 64 hex chars + newline + at least 1 byte data
            self.allocator.free(content);
            return null;
        }

        const signature = content[0..64];
        if (content[64] != '\n') {
            self.allocator.free(content);
            return null;
        }

        const data = content[65..];

        // Verify HMAC signature
        if (!self.verifyHmac(data, signature)) {
            self.allocator.free(content);
            return null;
        }

        // Return only the data portion (need to allocate new memory)
        const result = self.allocator.dupe(u8, data) catch {
            self.allocator.free(content);
            return null;
        };
        self.allocator.free(content);
        return result;
    }

    /// Store data in cache with HMAC signature
    pub fn put(self: *Self, key: []const u8, data: []const u8) !void {
        if (!self.enabled) return;

        // Extract directory from key
        const dir_end = std.mem.lastIndexOf(u8, key, "/") orelse return error.InvalidPath;
        const dir_path = key[0..dir_end];

        // Create directory structure with 0o755 permissions
        try ensureDirectory(dir_path);

        // Compute HMAC signature
        const signature = self.computeHmac(data);

        // Write file with 0o666 permissions
        const file = std.fs.createFileAbsolute(key, .{ .mode = 0o666 }) catch return error.WriteFileFailed;
        defer file.close();

        // Write signature + newline + data
        file.writeAll(&signature) catch return error.WriteFileFailed;
        file.writeAll("\n") catch return error.WriteFileFailed;
        file.writeAll(data) catch return error.WriteFileFailed;
    }

    /// Compute HMAC-SHA256 signature and return as hex string
    fn computeHmac(self: *Self, data: []const u8) [64]u8 {
        var mac: [HmacSha256.mac_length]u8 = undefined;
        HmacSha256.create(&mac, data, self.token);

        // Convert to hex string manually
        const hex_chars = "0123456789abcdef";
        var hex: [64]u8 = undefined;
        for (mac, 0..) |byte, i| {
            hex[i * 2] = hex_chars[byte >> 4];
            hex[i * 2 + 1] = hex_chars[byte & 0x0f];
        }
        return hex;
    }

    /// Verify HMAC-SHA256 signature
    fn verifyHmac(self: *Self, data: []const u8, signature: []const u8) bool {
        const expected = self.computeHmac(data);
        return std.mem.eql(u8, &expected, signature);
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

/// Set file/directory permissions using C chmod
fn setPermissions(path: []const u8, mode: std.c.mode_t) void {
    // Need null-terminated string for C function
    var buf: [4096]u8 = undefined;
    if (path.len >= buf.len) return;
    @memcpy(buf[0..path.len], path);
    buf[path.len] = 0;
    _ = std.c.chmod(@ptrCast(&buf), mode);
}

/// Ensure directory exists with 0o755 permissions
fn ensureDirectory(path: []const u8) !void {
    std.fs.makeDirAbsolute(path) catch |err| {
        switch (err) {
            error.PathAlreadyExists => {
                // Ensure permissions are correct
                setPermissions(path, 0o755);
                return;
            },
            error.FileNotFound => {
                // Parent directory doesn't exist, create recursively
                if (std.mem.lastIndexOf(u8, path, "/")) |parent_end| {
                    if (parent_end > 0) {
                        try ensureDirectory(path[0..parent_end]);
                        std.fs.makeDirAbsolute(path) catch |e| {
                            if (e != error.PathAlreadyExists) return e;
                        };
                        setPermissions(path, 0o755);
                    }
                }
                return;
            },
            else => return err,
        }
    };
    // Set permissions for newly created directory
    setPermissions(path, 0o755);
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
    const cache = Cache.init(allocator, "/tmp/cache", 300, "test_token");

    try std.testing.expect(cache.enabled);
    try std.testing.expectEqual(@as(i64, 300), cache.ttl_seconds);
    try std.testing.expectEqualStrings("test_token", cache.token);
}

test "Cache disabled when ttl is 0" {
    const allocator = std.testing.allocator;
    const cache = Cache.init(allocator, "/tmp/cache", 0, "test_token");

    try std.testing.expect(!cache.enabled);
}

test "computeHmac produces correct hex output" {
    const allocator = std.testing.allocator;
    var cache = Cache.init(allocator, "/tmp/cache", 300, "secret_key");

    const signature = cache.computeHmac("test data");
    try std.testing.expectEqual(@as(usize, 64), signature.len);

    // Verify it's valid hex
    for (signature) |c| {
        try std.testing.expect((c >= '0' and c <= '9') or (c >= 'a' and c <= 'f'));
    }
}

test "verifyHmac validates correct signature" {
    const allocator = std.testing.allocator;
    var cache = Cache.init(allocator, "/tmp/cache", 300, "secret_key");

    const data = "test data";
    const signature = cache.computeHmac(data);

    try std.testing.expect(cache.verifyHmac(data, &signature));
}

test "verifyHmac rejects incorrect signature" {
    const allocator = std.testing.allocator;
    var cache = Cache.init(allocator, "/tmp/cache", 300, "secret_key");

    const data = "test data";
    const wrong_signature = "0000000000000000000000000000000000000000000000000000000000000000";

    try std.testing.expect(!cache.verifyHmac(data, wrong_signature));
}

test "verifyHmac rejects tampered data" {
    const allocator = std.testing.allocator;
    var cache = Cache.init(allocator, "/tmp/cache", 300, "secret_key");

    const original_data = "test data";
    const signature = cache.computeHmac(original_data);
    const tampered_data = "tampered data";

    try std.testing.expect(!cache.verifyHmac(tampered_data, &signature));
}

test "generateKey produces shared path without euid" {
    const allocator = std.testing.allocator;
    // Use /tmp which always exists
    var cache = Cache.init(allocator, "/tmp", 300, "test_token");

    const key = try cache.generateKey("https://api.github.com/test", "abc123");
    defer allocator.free(key);

    // Verify path starts with cache_dir and encoded URL (not euid)
    try std.testing.expect(std.mem.startsWith(u8, key, "/tmp/https"));
    // Verify path structure: cache_dir/encoded_url-token_prefix
    try std.testing.expect(std.mem.indexOf(u8, key, "-abc123") != null);
    // Verify no numeric directory in path (would indicate euid)
    // Path should be like /tmp/https%3A%2F%2F...-abc123, not /tmp/1000/https%3A%2F%2F...-abc123
    const path_after_cache_dir = key[5..]; // skip "/tmp/"
    // First char should NOT be a digit (which would indicate euid directory)
    try std.testing.expect(path_after_cache_dir[0] == 'h'); // starts with 'https'
}

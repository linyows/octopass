const std = @import("std");
const Allocator = std.mem.Allocator;
const http = std.http;

const config_mod = @import("config.zig");
const cache_mod = @import("cache.zig");
const types = @import("types.zig");
const log = @import("log.zig");

/// GitLab API URL templates
/// Note: Group/subgroup paths with '/' must be URL-encoded as '%2F'
const group_members_url = "{s}groups/{s}/members?per_page=100";
const project_members_url = "{s}projects/{s}/members?per_page=100";
const user_keys_url = "{s}users/{d}/keys?per_page=100";
const user_search_url = "{s}users?username={s}";
const current_user_url = "{s}user";

pub const GitLabProvider = struct {
    allocator: Allocator,
    config: *const config_mod.Config,
    cache: cache_mod.Cache,
    logger: *log.Logger,

    const Self = @This();

    pub fn init(allocator: Allocator, config: *const config_mod.Config, logger: *log.Logger) Self {
        return .{
            .allocator = allocator,
            .config = config,
            .cache = cache_mod.Cache.init(allocator, types.default_cache_dir, config.cache, config.token),
            .logger = logger,
        };
    }

    pub fn deinit(self: *Self) void {
        _ = self;
    }

    /// Get members based on configuration mode
    pub fn getMembers(self: *Self, allocator: Allocator) types.ProviderError![]types.User {
        if (self.config.isRepositoryMode()) {
            return self.getProjectMembers(allocator);
        } else if (self.config.isTeamMode()) {
            return self.getSubgroupMembers(allocator);
        } else {
            return self.getGroupMembers(allocator);
        }
    }

    /// Get group members (Group only mode)
    fn getGroupMembers(self: *Self, allocator: Allocator) types.ProviderError![]types.User {
        const group = self.config.organization orelse return types.ProviderError.InvalidResponse;
        const encoded_path = urlEncodePath(allocator, group) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(encoded_path);

        const url = std.fmt.allocPrint(allocator, group_members_url, .{
            self.config.endpoint,
            encoded_path,
        }) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(url);

        const response = try self.httpGet(allocator, url, null);
        defer allocator.free(response);

        return self.parseUsersJson(allocator, response);
    }

    /// Get subgroup members (Group + Subgroup mode)
    fn getSubgroupMembers(self: *Self, allocator: Allocator) types.ProviderError![]types.User {
        const group = self.config.organization orelse return types.ProviderError.InvalidResponse;
        const subgroup = self.config.subgroup orelse return types.ProviderError.InvalidResponse;

        // Build path: group/subgroup
        const full_path = std.fmt.allocPrint(allocator, "{s}/{s}", .{ group, subgroup }) catch {
            return types.ProviderError.OutOfMemory;
        };
        defer allocator.free(full_path);

        const encoded_path = urlEncodePath(allocator, full_path) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(encoded_path);

        const url = std.fmt.allocPrint(allocator, group_members_url, .{
            self.config.endpoint,
            encoded_path,
        }) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(url);

        const response = try self.httpGet(allocator, url, null);
        defer allocator.free(response);

        return self.parseUsersJson(allocator, response);
    }

    /// Get project members (Group + Project mode)
    fn getProjectMembers(self: *Self, allocator: Allocator) types.ProviderError![]types.User {
        const group = self.config.organization orelse return types.ProviderError.InvalidResponse;
        const project = self.config.repository orelse return types.ProviderError.InvalidResponse;

        // Build path: group/project
        const full_path = std.fmt.allocPrint(allocator, "{s}/{s}", .{ group, project }) catch {
            return types.ProviderError.OutOfMemory;
        };
        defer allocator.free(full_path);

        const encoded_path = urlEncodePath(allocator, full_path) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(encoded_path);

        const url = std.fmt.allocPrint(allocator, project_members_url, .{
            self.config.endpoint,
            encoded_path,
        }) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(url);

        const response = try self.httpGet(allocator, url, null);
        defer allocator.free(response);

        return self.parseMembersWithPermission(allocator, response);
    }

    /// Get user's SSH public keys
    /// Note: GitLab requires user_id to get keys, so we first lookup the user
    pub fn getUserKeys(self: *Self, allocator: Allocator, username: []const u8) types.ProviderError![]const u8 {
        // First, get user_id from username
        const user_id = try self.getUserIdByUsername(allocator, username);

        const url = std.fmt.allocPrint(allocator, user_keys_url, .{
            self.config.endpoint,
            user_id,
        }) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(url);

        const response = try self.httpGet(allocator, url, null);
        defer allocator.free(response);

        return self.extractKeys(allocator, response);
    }

    /// Get user ID by username
    fn getUserIdByUsername(self: *Self, allocator: Allocator, username: []const u8) types.ProviderError!i64 {
        const url = std.fmt.allocPrint(allocator, user_search_url, .{
            self.config.endpoint,
            username,
        }) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(url);

        const response = try self.httpGet(allocator, url, null);
        defer allocator.free(response);

        const parsed = std.json.parseFromSlice(std.json.Value, allocator, response, .{}) catch {
            return types.ProviderError.JsonParseError;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) return types.ProviderError.JsonParseError;

        // Find user with matching username
        for (root.array.items) |item| {
            if (item != .object) continue;

            const username_value = item.object.get("username") orelse continue;
            if (username_value != .string) continue;

            if (std.mem.eql(u8, username_value.string, username)) {
                const id_value = item.object.get("id") orelse continue;
                if (id_value != .integer) continue;
                return id_value.integer;
            }
        }

        return types.ProviderError.UserNotFound;
    }

    /// Authenticate user with personal access token
    pub fn authenticate(self: *Self, username: []const u8, token: []const u8) types.ProviderError!bool {
        const url = std.fmt.allocPrint(self.allocator, current_user_url, .{
            self.config.endpoint,
        }) catch return types.ProviderError.OutOfMemory;
        defer self.allocator.free(url);

        // Make request with provided token, no cache
        const response = self.httpGetNoCache(self.allocator, url, token) catch return false;
        defer self.allocator.free(response);

        // Parse response and check username matches
        const parsed = std.json.parseFromSlice(std.json.Value, self.allocator, response, .{}) catch {
            return false;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .object) return false;

        const username_value = root.object.get("username") orelse return false;
        if (username_value != .string) return false;

        return std.mem.eql(u8, username_value.string, username);
    }

    /// Make HTTP GET request with caching
    fn httpGet(self: *Self, allocator: Allocator, url: []const u8, custom_token: ?[]const u8) types.ProviderError![]const u8 {
        // Try cache first
        const token_prefix = if (self.config.token.len >= 6) self.config.token[0..6] else self.config.token;
        const cache_key = self.cache.generateKey(url, token_prefix) catch null;
        defer if (cache_key) |k| self.allocator.free(k);

        if (cache_key) |key| {
            if (self.cache.get(key)) |cached| {
                defer self.allocator.free(cached);
                self.logger.info("cache hit: {s}", .{url});
                return allocator.dupe(u8, cached) catch return types.ProviderError.OutOfMemory;
            }
        }

        // Make HTTP request
        const response = try self.httpGetNoCache(allocator, url, custom_token);

        // Store in cache
        if (cache_key) |key| {
            self.cache.put(key, response) catch {};
        }

        return response;
    }

    /// Make HTTP GET request without caching
    fn httpGetNoCache(self: *Self, allocator: Allocator, url: []const u8, custom_token: ?[]const u8) types.ProviderError![]const u8 {
        self.logger.info("http get: {s}", .{url});

        var client = http.Client{ .allocator = allocator };
        defer client.deinit();

        const token = custom_token orelse self.config.token;

        const uri = std.Uri.parse(url) catch return types.ProviderError.HttpError;

        // GitLab uses PRIVATE-TOKEN header instead of Authorization
        var req = client.request(.GET, uri, .{
            .extra_headers = &.{
                .{ .name = "PRIVATE-TOKEN", .value = token },
                .{ .name = "User-Agent", .value = types.version_with_name },
                .{ .name = "Accept", .value = "application/json" },
                .{ .name = "Accept-Encoding", .value = "identity" },
            },
        }) catch return types.ProviderError.NetworkError;
        defer req.deinit();

        // Send bodiless request
        req.sendBodiless() catch return types.ProviderError.NetworkError;

        // Receive response head
        var redirect_buf: [8192]u8 = undefined;
        var response = req.receiveHead(&redirect_buf) catch return types.ProviderError.NetworkError;

        if (response.head.status != .ok) {
            self.logger.err("http error: status={d}", .{@intFromEnum(response.head.status)});
            if (response.head.status == .too_many_requests) {
                return types.ProviderError.RateLimited;
            }
            return types.ProviderError.HttpError;
        }

        // Read response body with decompression support
        var transfer_buf: [16384]u8 = undefined;
        var decompress: http.Decompress = undefined;
        var decompress_buf: [std.compress.flate.max_window_len]u8 = undefined;
        const reader = response.readerDecompressing(&transfer_buf, &decompress, &decompress_buf);
        const body = reader.allocRemaining(allocator, std.io.Limit.limited(types.max_buffer_size)) catch {
            return types.ProviderError.NetworkError;
        };

        self.logger.info("http response: {d} bytes", .{body.len});
        return body;
    }

    /// Parse users JSON array (GitLab format: uses "username" instead of "login")
    fn parseUsersJson(self: *Self, allocator: Allocator, json_data: []const u8) types.ProviderError![]types.User {
        _ = self;

        const parsed = std.json.parseFromSlice(std.json.Value, allocator, json_data, .{}) catch {
            return types.ProviderError.JsonParseError;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) return types.ProviderError.JsonParseError;

        var users = std.ArrayListUnmanaged(types.User){};
        errdefer {
            for (users.items) |*user| {
                user.deinit(allocator);
            }
            users.deinit(allocator);
        }

        for (root.array.items) |item| {
            if (item != .object) continue;

            const id_value = item.object.get("id") orelse continue;
            const username_value = item.object.get("username") orelse continue;

            if (id_value != .integer or username_value != .string) continue;

            const user = types.User{
                .id = id_value.integer,
                .login = allocator.dupe(u8, username_value.string) catch continue,
            };
            users.append(allocator, user) catch continue;
        }

        return users.toOwnedSlice(allocator) catch return types.ProviderError.OutOfMemory;
    }

    /// Parse members JSON with permission filtering based on access_level
    fn parseMembersWithPermission(self: *Self, allocator: Allocator, json_data: []const u8) types.ProviderError![]types.User {
        const parsed = std.json.parseFromSlice(std.json.Value, allocator, json_data, .{}) catch {
            return types.ProviderError.JsonParseError;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) return types.ProviderError.JsonParseError;

        const required_access_level = self.config.permission.toGitLabAccessLevel();

        var users = std.ArrayListUnmanaged(types.User){};
        errdefer {
            for (users.items) |*user| {
                user.deinit(allocator);
            }
            users.deinit(allocator);
        }

        for (root.array.items) |item| {
            if (item != .object) continue;

            // Check access_level
            const access_level_value = item.object.get("access_level") orelse continue;
            if (access_level_value != .integer) continue;

            // User must have at least the required access level
            if (access_level_value.integer < required_access_level) continue;

            // Extract user data
            const id_value = item.object.get("id") orelse continue;
            const username_value = item.object.get("username") orelse continue;

            if (id_value != .integer or username_value != .string) continue;

            const user = types.User{
                .id = id_value.integer,
                .login = allocator.dupe(u8, username_value.string) catch continue,
            };
            users.append(allocator, user) catch continue;
        }

        return users.toOwnedSlice(allocator) catch return types.ProviderError.OutOfMemory;
    }

    /// Extract SSH keys from JSON response
    fn extractKeys(self: *Self, allocator: Allocator, json_data: []const u8) types.ProviderError![]const u8 {
        _ = self;

        const parsed = std.json.parseFromSlice(std.json.Value, allocator, json_data, .{}) catch {
            return types.ProviderError.JsonParseError;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) return types.ProviderError.JsonParseError;

        var keys = std.ArrayListUnmanaged(u8){};
        errdefer keys.deinit(allocator);

        for (root.array.items) |item| {
            if (item != .object) continue;

            const key_value = item.object.get("key") orelse continue;
            if (key_value != .string) continue;

            keys.appendSlice(allocator, key_value.string) catch continue;
            keys.append(allocator, '\n') catch continue;
        }

        return keys.toOwnedSlice(allocator) catch return types.ProviderError.OutOfMemory;
    }
};

/// URL encode path segments (replace '/' with '%2F')
fn urlEncodePath(allocator: Allocator, path: []const u8) ![]const u8 {
    var result = std.ArrayListUnmanaged(u8){};
    errdefer result.deinit(allocator);

    for (path) |c| {
        if (c == '/') {
            try result.appendSlice(allocator, "%2F");
        } else {
            try result.append(allocator, c);
        }
    }

    return result.toOwnedSlice(allocator);
}

// Tests
test "GitLabProvider init" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();

    var logger = log.Logger.init("test", false);
    var gitlab = GitLabProvider.init(allocator, &config, &logger);
    defer gitlab.deinit();
}

test "urlEncodePath" {
    const allocator = std.testing.allocator;

    const result1 = try urlEncodePath(allocator, "mygroup");
    defer allocator.free(result1);
    try std.testing.expectEqualStrings("mygroup", result1);

    const result2 = try urlEncodePath(allocator, "mygroup/mysubgroup");
    defer allocator.free(result2);
    try std.testing.expectEqualStrings("mygroup%2Fmysubgroup", result2);

    const result3 = try urlEncodePath(allocator, "mygroup/mysubgroup/nested");
    defer allocator.free(result3);
    try std.testing.expectEqualStrings("mygroup%2Fmysubgroup%2Fnested", result3);
}

test "parseUsersJson" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();

    var logger = log.Logger.init("test", false);
    var gitlab = GitLabProvider.init(allocator, &config, &logger);
    defer gitlab.deinit();

    const json_data =
        \\[
        \\  {"id": 123, "username": "alice", "access_level": 30},
        \\  {"id": 456, "username": "bob", "access_level": 40}
        \\]
    ;

    const users = try gitlab.parseUsersJson(allocator, json_data);
    defer types.freeUsers(allocator, users);

    try std.testing.expectEqual(@as(usize, 2), users.len);
    try std.testing.expectEqual(@as(i64, 123), users[0].id);
    try std.testing.expectEqualStrings("alice", users[0].login);
    try std.testing.expectEqual(@as(i64, 456), users[1].id);
    try std.testing.expectEqualStrings("bob", users[1].login);
}

test "parseMembersWithPermission" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();
    config.permission = .write; // access_level >= 30

    var logger = log.Logger.init("test", false);
    var gitlab = GitLabProvider.init(allocator, &config, &logger);
    defer gitlab.deinit();

    const json_data =
        \\[
        \\  {"id": 1, "username": "guest", "access_level": 10},
        \\  {"id": 2, "username": "reporter", "access_level": 20},
        \\  {"id": 3, "username": "developer", "access_level": 30},
        \\  {"id": 4, "username": "maintainer", "access_level": 40}
        \\]
    ;

    const users = try gitlab.parseMembersWithPermission(allocator, json_data);
    defer types.freeUsers(allocator, users);

    // Only developer (30) and maintainer (40) should be included
    try std.testing.expectEqual(@as(usize, 2), users.len);
    try std.testing.expectEqualStrings("developer", users[0].login);
    try std.testing.expectEqualStrings("maintainer", users[1].login);
}

test "extractKeys" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();

    var logger = log.Logger.init("test", false);
    var gitlab = GitLabProvider.init(allocator, &config, &logger);
    defer gitlab.deinit();

    const json_data =
        \\[
        \\  {"id": 1, "key": "ssh-rsa AAAA... user@host1", "title": "key1"},
        \\  {"id": 2, "key": "ssh-ed25519 BBBB... user@host2", "title": "key2"}
        \\]
    ;

    const keys = try gitlab.extractKeys(allocator, json_data);
    defer allocator.free(keys);

    try std.testing.expect(std.mem.indexOf(u8, keys, "ssh-rsa AAAA") != null);
    try std.testing.expect(std.mem.indexOf(u8, keys, "ssh-ed25519 BBBB") != null);
}

const std = @import("std");
const Allocator = std.mem.Allocator;
const http = std.http;

const config_mod = @import("config.zig");
const cache_mod = @import("cache.zig");
const types = @import("types.zig");
const log = @import("log.zig");

/// GitHub API URL templates
const teams_url = "{s}orgs/{s}/teams?per_page=100";
const team_members_url = "{s}teams/{d}/members?per_page=100";
const collaborators_url = "{s}repos/{s}/{s}/collaborators?per_page=100";
const user_keys_url = "{s}users/{s}/keys?per_page=100";
const user_url = "{s}user";

pub const GitHubProvider = struct {
    allocator: Allocator,
    config: *const config_mod.Config,
    cache: cache_mod.Cache,
    logger: *log.Logger,

    const Self = @This();

    pub fn init(allocator: Allocator, config: *const config_mod.Config, logger: *log.Logger) Self {
        return .{
            .allocator = allocator,
            .config = config,
            .cache = cache_mod.Cache.init(allocator, types.default_cache_dir, config.cache),
            .logger = logger,
        };
    }

    pub fn deinit(self: *Self) void {
        _ = self;
    }

    /// Get members (team members or repository collaborators)
    pub fn getMembers(self: *Self, allocator: Allocator) types.ProviderError![]types.User {
        if (self.config.isRepositoryMode()) {
            return self.getRepositoryCollaborators(allocator);
        } else {
            return self.getTeamMembers(allocator);
        }
    }

    /// Get team members
    fn getTeamMembers(self: *Self, allocator: Allocator) types.ProviderError![]types.User {
        const team_id = try self.getTeamIdByName(self.config.team.?);

        const url = std.fmt.allocPrint(allocator, team_members_url, .{
            self.config.endpoint,
            team_id,
        }) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(url);

        const response = try self.httpGet(allocator, url, null);
        defer allocator.free(response);

        return self.parseUsersJson(allocator, response);
    }

    /// Get repository collaborators
    fn getRepositoryCollaborators(self: *Self, allocator: Allocator) types.ProviderError![]types.User {
        const url = std.fmt.allocPrint(allocator, collaborators_url, .{
            self.config.endpoint,
            self.config.owner.?,
            self.config.repository.?,
        }) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(url);

        const response = try self.httpGet(allocator, url, null);
        defer allocator.free(response);

        return self.parseCollaboratorsJson(allocator, response);
    }

    /// Get teams for organization
    pub fn getTeams(self: *Self, allocator: Allocator) types.ProviderError![]types.Team {
        const url = std.fmt.allocPrint(allocator, teams_url, .{
            self.config.endpoint,
            self.config.organization.?,
        }) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(url);

        const response = try self.httpGet(allocator, url, null);
        defer allocator.free(response);

        return self.parseTeamsJson(allocator, response);
    }

    /// Get team ID by name or slug
    pub fn getTeamIdByName(self: *Self, team_name: []const u8) types.ProviderError!i64 {
        const teams = try self.getTeams(self.allocator);
        defer types.freeTeams(self.allocator, teams);

        for (teams) |team| {
            if (std.mem.eql(u8, team.name, team_name) or std.mem.eql(u8, team.slug, team_name)) {
                return team.id;
            }
        }

        return types.ProviderError.TeamNotFound;
    }

    /// Get user's SSH public keys
    pub fn getUserKeys(self: *Self, allocator: Allocator, username: []const u8) types.ProviderError![]const u8 {
        const url = std.fmt.allocPrint(allocator, user_keys_url, .{
            self.config.endpoint,
            username,
        }) catch return types.ProviderError.OutOfMemory;
        defer allocator.free(url);

        const response = try self.httpGet(allocator, url, null);
        defer allocator.free(response);

        return self.extractKeys(allocator, response);
    }

    /// Authenticate user with personal access token
    pub fn authenticate(self: *Self, username: []const u8, token: []const u8) types.ProviderError!bool {
        const url = std.fmt.allocPrint(self.allocator, user_url, .{
            self.config.endpoint,
        }) catch return types.ProviderError.OutOfMemory;
        defer self.allocator.free(url);

        // Make request with provided token, no cache
        const response = self.httpGetNoCache(self.allocator, url, token) catch return false;
        defer self.allocator.free(response);

        // Parse response and check login matches
        const parsed = std.json.parseFromSlice(std.json.Value, self.allocator, response, .{}) catch {
            return false;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .object) return false;

        const login_value = root.object.get("login") orelse return false;
        if (login_value != .string) return false;

        return std.mem.eql(u8, login_value.string, username);
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
        const auth_header = std.fmt.allocPrint(allocator, "token {s}", .{token}) catch {
            return types.ProviderError.OutOfMemory;
        };
        defer allocator.free(auth_header);

        const uri = std.Uri.parse(url) catch return types.ProviderError.HttpError;

        var req = client.request(.GET, uri, .{
            .extra_headers = &.{
                .{ .name = "Authorization", .value = auth_header },
                .{ .name = "User-Agent", .value = types.version_with_name },
                .{ .name = "Accept", .value = "application/vnd.github.v3+json" },
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

    /// Parse users JSON array
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
            const login_value = item.object.get("login") orelse continue;

            if (id_value != .integer or login_value != .string) continue;

            const user = types.User{
                .id = id_value.integer,
                .login = allocator.dupe(u8, login_value.string) catch continue,
            };
            users.append(allocator, user) catch continue;
        }

        return users.toOwnedSlice(allocator) catch return types.ProviderError.OutOfMemory;
    }

    /// Parse collaborators JSON with permission filtering
    fn parseCollaboratorsJson(self: *Self, allocator: Allocator, json_data: []const u8) types.ProviderError![]types.User {
        const parsed = std.json.parseFromSlice(std.json.Value, allocator, json_data, .{}) catch {
            return types.ProviderError.JsonParseError;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) return types.ProviderError.JsonParseError;

        const required_permission = self.config.permission.toString();

        var users = std.ArrayListUnmanaged(types.User){};
        errdefer {
            for (users.items) |*user| {
                user.deinit(allocator);
            }
            users.deinit(allocator);
        }

        for (root.array.items) |item| {
            if (item != .object) continue;

            // Check permission
            const permissions_value = item.object.get("permissions") orelse continue;
            if (permissions_value != .object) continue;

            const has_permission = permissions_value.object.get(required_permission) orelse continue;
            if (has_permission != .bool or !has_permission.bool) continue;

            // Extract user data
            const id_value = item.object.get("id") orelse continue;
            const login_value = item.object.get("login") orelse continue;

            if (id_value != .integer or login_value != .string) continue;

            const user = types.User{
                .id = id_value.integer,
                .login = allocator.dupe(u8, login_value.string) catch continue,
            };
            users.append(allocator, user) catch continue;
        }

        return users.toOwnedSlice(allocator) catch return types.ProviderError.OutOfMemory;
    }

    /// Parse teams JSON
    fn parseTeamsJson(self: *Self, allocator: Allocator, json_data: []const u8) types.ProviderError![]types.Team {
        _ = self;

        const parsed = std.json.parseFromSlice(std.json.Value, allocator, json_data, .{}) catch {
            return types.ProviderError.JsonParseError;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) return types.ProviderError.JsonParseError;

        var teams = std.ArrayListUnmanaged(types.Team){};
        errdefer {
            for (teams.items) |*team| {
                team.deinit(allocator);
            }
            teams.deinit(allocator);
        }

        for (root.array.items) |item| {
            if (item != .object) continue;

            const id_value = item.object.get("id") orelse continue;
            const name_value = item.object.get("name") orelse continue;
            const slug_value = item.object.get("slug") orelse continue;

            if (id_value != .integer or name_value != .string or slug_value != .string) continue;

            const team = types.Team{
                .id = id_value.integer,
                .name = allocator.dupe(u8, name_value.string) catch continue,
                .slug = allocator.dupe(u8, slug_value.string) catch continue,
            };
            teams.append(allocator, team) catch continue;
        }

        return teams.toOwnedSlice(allocator) catch return types.ProviderError.OutOfMemory;
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

// Tests
test "GitHubProvider init" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();

    var logger = log.Logger.init("test", false);
    var github = GitHubProvider.init(allocator, &config, &logger);
    defer github.deinit();
}

test "parseUsersJson" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();

    var logger = log.Logger.init("test", false);
    var github = GitHubProvider.init(allocator, &config, &logger);
    defer github.deinit();

    const json_data =
        \\[
        \\  {"id": 123, "login": "alice"},
        \\  {"id": 456, "login": "bob"}
        \\]
    ;

    const users = try github.parseUsersJson(allocator, json_data);
    defer types.freeUsers(allocator, users);

    try std.testing.expectEqual(@as(usize, 2), users.len);
    try std.testing.expectEqual(@as(i64, 123), users[0].id);
    try std.testing.expectEqualStrings("alice", users[0].login);
    try std.testing.expectEqual(@as(i64, 456), users[1].id);
    try std.testing.expectEqualStrings("bob", users[1].login);
}

test "parseTeamsJson" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();

    var logger = log.Logger.init("test", false);
    var github = GitHubProvider.init(allocator, &config, &logger);
    defer github.deinit();

    const json_data =
        \\[
        \\  {"id": 1, "name": "Core Team", "slug": "core-team"},
        \\  {"id": 2, "name": "Developers", "slug": "developers"}
        \\]
    ;

    const teams = try github.parseTeamsJson(allocator, json_data);
    defer types.freeTeams(allocator, teams);

    try std.testing.expectEqual(@as(usize, 2), teams.len);
    try std.testing.expectEqual(@as(i64, 1), teams[0].id);
    try std.testing.expectEqualStrings("Core Team", teams[0].name);
    try std.testing.expectEqualStrings("core-team", teams[0].slug);
}

test "extractKeys" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();

    var logger = log.Logger.init("test", false);
    var github = GitHubProvider.init(allocator, &config, &logger);
    defer github.deinit();

    const json_data =
        \\[
        \\  {"id": 1, "key": "ssh-rsa AAAA... user@host1"},
        \\  {"id": 2, "key": "ssh-ed25519 BBBB... user@host2"}
        \\]
    ;

    const keys = try github.extractKeys(allocator, json_data);
    defer allocator.free(keys);

    try std.testing.expect(std.mem.indexOf(u8, keys, "ssh-rsa AAAA") != null);
    try std.testing.expect(std.mem.indexOf(u8, keys, "ssh-ed25519 BBBB") != null);
}

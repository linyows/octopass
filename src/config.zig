const std = @import("std");
const types = @import("types.zig");
const Allocator = std.mem.Allocator;

pub const Config = struct {
    allocator: Allocator,

    // Provider settings
    provider: types.ProviderType = .github,

    // API settings
    endpoint: []const u8,
    token: []const u8,

    // Organization/Team mode
    organization: ?[]const u8 = null,
    team: ?[]const u8 = null,

    // Repository collaborator mode
    owner: ?[]const u8 = null,
    repository: ?[]const u8 = null,
    permission: types.Permission = .write,

    // User settings
    group_name: ?[]const u8 = null,
    home: []const u8,
    shell: []const u8,
    uid_starts: i64 = 2000,
    gid: i64 = 2000,

    // Cache settings
    cache: i64 = 500,

    // Logging
    syslog: bool = false,

    // Shared users
    shared_users: [][]const u8,

    // Track which fields were allocated
    endpoint_allocated: bool = false,
    token_allocated: bool = false,
    organization_allocated: bool = false,
    team_allocated: bool = false,
    owner_allocated: bool = false,
    repository_allocated: bool = false,
    group_name_allocated: bool = false,
    home_allocated: bool = false,
    shell_allocated: bool = false,
    shared_users_allocated: bool = false,

    const Self = @This();

    pub fn init(allocator: Allocator) Self {
        return .{
            .allocator = allocator,
            .endpoint = types.default_api_endpoint,
            .token = "",
            .home = "/home/%s",
            .shell = "/bin/bash",
            .shared_users = &[_][]const u8{},
        };
    }

    pub fn deinit(self: *Self) void {
        if (self.endpoint_allocated) self.allocator.free(self.endpoint);
        if (self.token_allocated) self.allocator.free(self.token);
        if (self.organization_allocated) if (self.organization) |o| self.allocator.free(o);
        if (self.team_allocated) if (self.team) |t| self.allocator.free(t);
        if (self.owner_allocated) if (self.owner) |o| self.allocator.free(o);
        if (self.repository_allocated) if (self.repository) |r| self.allocator.free(r);
        if (self.group_name_allocated) if (self.group_name) |g| self.allocator.free(g);
        if (self.home_allocated) self.allocator.free(self.home);
        if (self.shell_allocated) self.allocator.free(self.shell);
        if (self.shared_users_allocated) {
            for (self.shared_users) |user| {
                self.allocator.free(user);
            }
            self.allocator.free(self.shared_users);
        }
    }

    /// Load configuration from file
    pub fn load(allocator: Allocator, filename: []const u8) !Self {
        var config = Self.init(allocator);
        errdefer config.deinit();

        const file = std.fs.openFileAbsolute(filename, .{}) catch |err| {
            if (err == error.FileNotFound) {
                return error.ConfigNotFound;
            }
            return err;
        };
        defer file.close();

        const content = try file.readToEndAlloc(allocator, types.max_buffer_size);
        defer allocator.free(content);

        try config.parse(content);
        try config.overrideFromEnv();
        try config.applyDefaults();

        return config;
    }

    /// Parse configuration content
    fn parse(self: *Self, content: []const u8) !void {
        var lines = std.mem.splitScalar(u8, content, '\n');

        while (lines.next()) |line| {
            const trimmed = std.mem.trim(u8, line, " \t\r");

            // Skip empty lines and comments
            if (trimmed.len == 0 or trimmed[0] == '#') continue;

            // Find delimiter
            const delim_pos = std.mem.indexOf(u8, trimmed, "=") orelse continue;
            if (delim_pos == 0) continue;

            const key = std.mem.trim(u8, trimmed[0..delim_pos], " \t");
            var value = std.mem.trim(u8, trimmed[delim_pos + 1 ..], " \t");

            // Handle SharedUsers specially (array format)
            if (std.mem.eql(u8, key, "SharedUsers")) {
                try self.parseSharedUsers(value);
                continue;
            }

            // Remove quotes from value
            value = removeQuotes(value);

            // Set config value
            try self.setValue(key, value);
        }
    }

    /// Set a single config value
    fn setValue(self: *Self, key: []const u8, value: []const u8) !void {
        if (std.mem.eql(u8, key, "Provider")) {
            self.provider = types.ProviderType.fromString(value) orelse .github;
        } else if (std.mem.eql(u8, key, "Endpoint")) {
            self.endpoint = try normalizeUrl(self.allocator, value);
            self.endpoint_allocated = true;
        } else if (std.mem.eql(u8, key, "Token")) {
            self.token = try self.allocator.dupe(u8, value);
            self.token_allocated = true;
        } else if (std.mem.eql(u8, key, "Organization")) {
            self.organization = try self.allocator.dupe(u8, value);
            self.organization_allocated = true;
        } else if (std.mem.eql(u8, key, "Team")) {
            self.team = try self.allocator.dupe(u8, value);
            self.team_allocated = true;
        } else if (std.mem.eql(u8, key, "Owner")) {
            self.owner = try self.allocator.dupe(u8, value);
            self.owner_allocated = true;
        } else if (std.mem.eql(u8, key, "Repository")) {
            self.repository = try self.allocator.dupe(u8, value);
            self.repository_allocated = true;
        } else if (std.mem.eql(u8, key, "Permission")) {
            self.permission = types.Permission.fromString(value) orelse .write;
        } else if (std.mem.eql(u8, key, "Group")) {
            self.group_name = try self.allocator.dupe(u8, value);
            self.group_name_allocated = true;
        } else if (std.mem.eql(u8, key, "Home")) {
            self.home = try self.allocator.dupe(u8, value);
            self.home_allocated = true;
        } else if (std.mem.eql(u8, key, "Shell")) {
            self.shell = try self.allocator.dupe(u8, value);
            self.shell_allocated = true;
        } else if (std.mem.eql(u8, key, "UidStarts")) {
            self.uid_starts = std.fmt.parseInt(i64, value, 10) catch 2000;
        } else if (std.mem.eql(u8, key, "Gid")) {
            self.gid = std.fmt.parseInt(i64, value, 10) catch 2000;
        } else if (std.mem.eql(u8, key, "Cache")) {
            self.cache = std.fmt.parseInt(i64, value, 10) catch 500;
        } else if (std.mem.eql(u8, key, "Syslog")) {
            self.syslog = std.mem.eql(u8, value, "true");
        }
    }

    /// Parse SharedUsers array format: [ "user1", "user2" ]
    fn parseSharedUsers(self: *Self, value: []const u8) !void {
        var users = std.ArrayListUnmanaged([]const u8){};
        errdefer {
            for (users.items) |user| {
                self.allocator.free(user);
            }
            users.deinit(self.allocator);
        }

        // Find content between brackets
        const start = std.mem.indexOf(u8, value, "[") orelse return;
        const end = std.mem.lastIndexOf(u8, value, "]") orelse return;
        if (start >= end) return;

        const content = value[start + 1 .. end];
        var parts = std.mem.splitScalar(u8, content, ',');

        while (parts.next()) |part| {
            const trimmed = std.mem.trim(u8, part, " \t\"'");
            if (trimmed.len > 0) {
                const user = try self.allocator.dupe(u8, trimmed);
                try users.append(self.allocator, user);
            }
        }

        self.shared_users = try users.toOwnedSlice(self.allocator);
        self.shared_users_allocated = true;
    }

    /// Apply default values
    fn applyDefaults(self: *Self) !void {
        // Set group_name to team or repository if not set
        if (self.group_name == null) {
            if (self.repository) |repo| {
                self.group_name = try self.allocator.dupe(u8, repo);
                self.group_name_allocated = true;
            } else if (self.team) |team| {
                self.group_name = try self.allocator.dupe(u8, team);
                self.group_name_allocated = true;
            }
        }

        // Set owner to organization if not set
        if (self.owner == null and self.organization != null) {
            self.owner = try self.allocator.dupe(u8, self.organization.?);
            self.owner_allocated = true;
        }
    }

    /// Override config values from environment variables
    fn overrideFromEnv(self: *Self) !void {
        const env_vars = [_]struct { env: []const u8, field: []const u8 }{
            .{ .env = "OCTOPASS_TOKEN", .field = "Token" },
            .{ .env = "OCTOPASS_ENDPOINT", .field = "Endpoint" },
            .{ .env = "OCTOPASS_ORGANIZATION", .field = "Organization" },
            .{ .env = "OCTOPASS_TEAM", .field = "Team" },
            .{ .env = "OCTOPASS_OWNER", .field = "Owner" },
            .{ .env = "OCTOPASS_REPOSITORY", .field = "Repository" },
            .{ .env = "OCTOPASS_PERMISSION", .field = "Permission" },
        };

        for (env_vars) |ev| {
            if (std.posix.getenv(ev.env)) |value| {
                try self.setValue(ev.field, value);
            }
        }
    }

    /// Check if config is valid
    pub fn validate(self: *const Self) bool {
        if (self.token.len == 0) return false;

        // Must have either team or repository mode
        const has_team = self.organization != null and self.team != null;
        const has_repo = self.owner != null and self.repository != null;

        return has_team or has_repo;
    }

    /// Check if using team mode
    pub fn isTeamMode(self: *const Self) bool {
        return self.organization != null and self.team != null;
    }

    /// Check if using repository collaborator mode
    pub fn isRepositoryMode(self: *const Self) bool {
        return self.repository != null;
    }

    /// Mask token for logging
    pub fn maskedToken(self: *const Self) []const u8 {
        if (self.token.len <= 5) return "***";
        return self.token[0..5];
    }
};

/// Remove quotes from string
fn removeQuotes(s: []const u8) []const u8 {
    if (s.len < 2) return s;

    var result = s;
    if (result[0] == '"' and result[result.len - 1] == '"') {
        result = result[1 .. result.len - 1];
    } else if (result[0] == '\'' and result[result.len - 1] == '\'') {
        result = result[1 .. result.len - 1];
    }

    return result;
}

/// Normalize URL to have trailing slash
fn normalizeUrl(allocator: Allocator, url: []const u8) ![]const u8 {
    if (url.len == 0) return types.default_api_endpoint;

    if (url[url.len - 1] == '/') {
        return try allocator.dupe(u8, url);
    }

    const result = try allocator.alloc(u8, url.len + 1);
    @memcpy(result[0..url.len], url);
    result[url.len] = '/';
    return result;
}

// Tests
test "Config init" {
    const allocator = std.testing.allocator;
    var config = Config.init(allocator);
    defer config.deinit();

    try std.testing.expectEqualStrings(types.default_api_endpoint, config.endpoint);
    try std.testing.expectEqual(@as(i64, 2000), config.uid_starts);
    try std.testing.expectEqual(@as(i64, 2000), config.gid);
    try std.testing.expectEqual(@as(i64, 500), config.cache);
    try std.testing.expectEqual(false, config.syslog);
}

test "removeQuotes" {
    try std.testing.expectEqualStrings("test", removeQuotes("\"test\""));
    try std.testing.expectEqualStrings("test", removeQuotes("'test'"));
    try std.testing.expectEqualStrings("test", removeQuotes("test"));
    try std.testing.expectEqualStrings("", removeQuotes("\"\""));
}

test "normalizeUrl" {
    const allocator = std.testing.allocator;

    const url1 = try normalizeUrl(allocator, "https://api.github.com");
    defer allocator.free(url1);
    try std.testing.expectEqualStrings("https://api.github.com/", url1);

    const url2 = try normalizeUrl(allocator, "https://api.github.com/");
    defer allocator.free(url2);
    try std.testing.expectEqualStrings("https://api.github.com/", url2);
}

test "Config parse simple" {
    const allocator = std.testing.allocator;

    const content =
        \\Token = "ghp_test123"
        \\Organization = "myorg"
        \\Team = "myteam"
        \\UidStarts = 3000
        \\Syslog = true
    ;

    var config = Config.init(allocator);
    defer config.deinit();

    try config.parse(content);
    try config.applyDefaults();

    try std.testing.expectEqualStrings("ghp_test123", config.token);
    try std.testing.expectEqualStrings("myorg", config.organization.?);
    try std.testing.expectEqualStrings("myteam", config.team.?);
    try std.testing.expectEqual(@as(i64, 3000), config.uid_starts);
    try std.testing.expectEqual(true, config.syslog);
    try std.testing.expect(config.validate());
}

test "Config parse SharedUsers" {
    const allocator = std.testing.allocator;

    const content =
        \\Token = "test"
        \\Organization = "org"
        \\Team = "team"
        \\SharedUsers = [ "admin", "deploy" ]
    ;

    var config = Config.init(allocator);
    defer config.deinit();

    try config.parse(content);
    try config.applyDefaults();

    try std.testing.expectEqual(@as(usize, 2), config.shared_users.len);
    try std.testing.expectEqualStrings("admin", config.shared_users[0]);
    try std.testing.expectEqualStrings("deploy", config.shared_users[1]);
}

test "Config validate" {
    const allocator = std.testing.allocator;

    var config = Config.init(allocator);
    defer config.deinit();

    // Empty config is invalid
    try std.testing.expect(!config.validate());

    // Token only is still invalid
    config.token = "test";
    try std.testing.expect(!config.validate());

    // With organization and team is valid
    config.organization = "org";
    config.team = "team";
    try std.testing.expect(config.validate());
}

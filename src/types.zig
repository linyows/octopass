const std = @import("std");
const Allocator = std.mem.Allocator;
const build_options = @import("build_options");

pub const version: []const u8 = if (@hasDecl(build_options, "version")) build_options.version else "0.0.0-dev";
pub const version_with_name = "octopass/" ++ version;

pub const default_config_file = "/etc/octopass.conf";
pub const default_cache_dir = "/var/cache/octopass";
pub const default_github_endpoint = "https://api.github.com/";
pub const default_gitlab_endpoint = "https://gitlab.com/api/v4/";
pub const default_api_endpoint = default_github_endpoint; // For backward compatibility
pub const max_buffer_size: usize = 10 * 1024 * 1024; // 10MB

/// GitHub/Provider user representation
pub const User = struct {
    id: i64,
    login: []const u8,
    keys: ?[]const u8 = null,

    pub fn deinit(self: *User, allocator: Allocator) void {
        allocator.free(self.login);
        if (self.keys) |keys| {
            allocator.free(keys);
        }
    }
};

/// Team information
pub const Team = struct {
    id: i64,
    name: []const u8,
    slug: []const u8,

    pub fn deinit(self: *Team, allocator: Allocator) void {
        allocator.free(self.name);
        allocator.free(self.slug);
    }
};

/// HTTP response
pub const Response = struct {
    data: []const u8,
    http_status: u16,
    allocator: Allocator,

    pub fn deinit(self: *Response) void {
        self.allocator.free(self.data);
    }
};

/// Provider type enumeration
pub const ProviderType = enum {
    github,
    gitlab,

    pub fn fromString(s: []const u8) ?ProviderType {
        if (std.mem.eql(u8, s, "github")) return .github;
        if (std.mem.eql(u8, s, "gitlab")) return .gitlab;
        return null;
    }

    pub fn toString(self: ProviderType) []const u8 {
        return switch (self) {
            .github => "github",
            .gitlab => "gitlab",
        };
    }

    pub fn defaultEndpoint(self: ProviderType) []const u8 {
        return switch (self) {
            .github => default_github_endpoint,
            .gitlab => default_gitlab_endpoint,
        };
    }
};

/// Permission level for repository collaborators
pub const Permission = enum {
    admin,
    write,
    read,

    pub fn fromString(s: []const u8) ?Permission {
        if (std.mem.eql(u8, s, "admin")) return .admin;
        if (std.mem.eql(u8, s, "write")) return .write;
        if (std.mem.eql(u8, s, "read")) return .read;
        return null;
    }

    /// Returns GitHub permission string (admin, push, pull)
    pub fn toString(self: Permission) []const u8 {
        return switch (self) {
            .admin => "admin",
            .write => "push",
            .read => "pull",
        };
    }

    /// Convert to GitLab access level
    /// GitLab access levels: 10=Guest, 20=Reporter, 30=Developer, 40=Maintainer, 50=Owner
    pub fn toGitLabAccessLevel(self: Permission) i64 {
        return switch (self) {
            .admin => 40, // Maintainer
            .write => 30, // Developer
            .read => 20, // Reporter
        };
    }
};

/// NSS status codes (matching C enum nss_status)
pub const NssStatus = enum(c_int) {
    success = 1,
    notfound = 0,
    unavail = -1,
    tryagain = -2,
    @"return" = -3,
};

/// Error types for octopass operations
pub const OctopassError = error{
    ConfigNotFound,
    ConfigParseError,
    InvalidConfig,
    HttpError,
    JsonParseError,
    CacheError,
    AuthenticationFailed,
    TeamNotFound,
    UserNotFound,
    OutOfMemory,
    BufferTooSmall,
    NetworkError,
    Timeout,
    InvalidUrl,
};

/// Provider errors
pub const ProviderError = error{
    HttpError,
    JsonParseError,
    TeamNotFound,
    UserNotFound,
    AuthenticationFailed,
    NetworkError,
    Timeout,
    OutOfMemory,
    InvalidResponse,
    RateLimited,
};

/// Find user by login name in user slice
pub fn findUserByLogin(users: []const User, login: []const u8) ?*const User {
    for (users) |*user| {
        if (std.mem.eql(u8, user.login, login)) {
            return user;
        }
    }
    return null;
}

/// Find user by ID in user slice
pub fn findUserById(users: []const User, id: i64) ?*const User {
    for (users) |*user| {
        if (user.id == id) {
            return user;
        }
    }
    return null;
}

/// Free a slice of users
pub fn freeUsers(allocator: Allocator, users: []User) void {
    for (users) |*user| {
        user.deinit(allocator);
    }
    allocator.free(users);
}

/// Free a slice of teams
pub fn freeTeams(allocator: Allocator, teams: []Team) void {
    for (teams) |*team| {
        team.deinit(allocator);
    }
    allocator.free(teams);
}

test "ProviderType fromString" {
    try std.testing.expectEqual(ProviderType.github, ProviderType.fromString("github").?);
    try std.testing.expectEqual(ProviderType.gitlab, ProviderType.fromString("gitlab").?);
    try std.testing.expectEqual(@as(?ProviderType, null), ProviderType.fromString("unknown"));
}

test "ProviderType defaultEndpoint" {
    try std.testing.expectEqualStrings(default_github_endpoint, ProviderType.github.defaultEndpoint());
    try std.testing.expectEqualStrings(default_gitlab_endpoint, ProviderType.gitlab.defaultEndpoint());
}

test "Permission fromString" {
    try std.testing.expectEqual(Permission.admin, Permission.fromString("admin").?);
    try std.testing.expectEqual(Permission.write, Permission.fromString("write").?);
    try std.testing.expectEqual(Permission.read, Permission.fromString("read").?);
    try std.testing.expectEqual(@as(?Permission, null), Permission.fromString("unknown"));
}

test "Permission toString" {
    try std.testing.expectEqualStrings("admin", Permission.admin.toString());
    try std.testing.expectEqualStrings("push", Permission.write.toString());
    try std.testing.expectEqualStrings("pull", Permission.read.toString());
}

test "Permission toGitLabAccessLevel" {
    try std.testing.expectEqual(@as(i64, 40), Permission.admin.toGitLabAccessLevel());
    try std.testing.expectEqual(@as(i64, 30), Permission.write.toGitLabAccessLevel());
    try std.testing.expectEqual(@as(i64, 20), Permission.read.toGitLabAccessLevel());
}

test "findUserByLogin" {
    var users = [_]User{
        .{ .id = 1, .login = "alice" },
        .{ .id = 2, .login = "bob" },
        .{ .id = 3, .login = "charlie" },
    };

    const found = findUserByLogin(&users, "bob");
    try std.testing.expect(found != null);
    try std.testing.expectEqual(@as(i64, 2), found.?.id);

    const not_found = findUserByLogin(&users, "david");
    try std.testing.expect(not_found == null);
}

test "findUserById" {
    var users = [_]User{
        .{ .id = 1, .login = "alice" },
        .{ .id = 2, .login = "bob" },
        .{ .id = 3, .login = "charlie" },
    };

    const found = findUserById(&users, 2);
    try std.testing.expect(found != null);
    try std.testing.expectEqualStrings("bob", found.?.login);

    const not_found = findUserById(&users, 999);
    try std.testing.expect(not_found == null);
}

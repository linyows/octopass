const std = @import("std");
const Allocator = std.mem.Allocator;

const config_mod = @import("config.zig");
const types = @import("types.zig");
const log = @import("log.zig");
const github_mod = @import("github.zig");
const gitlab_mod = @import("gitlab.zig");

/// Unified provider interface using tagged union
pub const Provider = union(types.ProviderType) {
    github: github_mod.GitHubProvider,
    gitlab: gitlab_mod.GitLabProvider,

    const Self = @This();

    /// Initialize provider based on config
    pub fn init(allocator: Allocator, config: *const config_mod.Config, logger: *log.Logger) Self {
        return switch (config.provider) {
            .github => .{ .github = github_mod.GitHubProvider.init(allocator, config, logger) },
            .gitlab => .{ .gitlab = gitlab_mod.GitLabProvider.init(allocator, config, logger) },
        };
    }

    pub fn deinit(self: *Self) void {
        switch (self.*) {
            .github => |*g| g.deinit(),
            .gitlab => |*g| g.deinit(),
        }
    }

    /// Get members (team members, repository collaborators, group members, etc.)
    pub fn getMembers(self: *Self, allocator: Allocator) types.ProviderError![]types.User {
        return switch (self.*) {
            .github => |*g| g.getMembers(allocator),
            .gitlab => |*g| g.getMembers(allocator),
        };
    }

    /// Get user's SSH public keys
    pub fn getUserKeys(self: *Self, allocator: Allocator, username: []const u8) types.ProviderError![]const u8 {
        return switch (self.*) {
            .github => |*g| g.getUserKeys(allocator, username),
            .gitlab => |*g| g.getUserKeys(allocator, username),
        };
    }

    /// Authenticate user with personal access token
    pub fn authenticate(self: *Self, username: []const u8, token: []const u8) types.ProviderError!bool {
        return switch (self.*) {
            .github => |*g| g.authenticate(username, token),
            .gitlab => |*g| g.authenticate(username, token),
        };
    }
};

// Tests
test "Provider init GitHub" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();
    config.provider = .github;

    var logger = log.Logger.init("test", false);
    var provider = Provider.init(allocator, &config, &logger);
    defer provider.deinit();

    try std.testing.expectEqual(types.ProviderType.github, @as(types.ProviderType, provider));
}

test "Provider init GitLab" {
    const allocator = std.testing.allocator;

    var config = config_mod.Config.init(allocator);
    defer config.deinit();
    config.provider = .gitlab;

    var logger = log.Logger.init("test", false);
    var provider = Provider.init(allocator, &config, &logger);
    defer provider.deinit();

    try std.testing.expectEqual(types.ProviderType.gitlab, @as(types.ProviderType, provider));
}

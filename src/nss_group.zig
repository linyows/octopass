const std = @import("std");
const common = @import("nss_common.zig");
const types = @import("types.zig");
const config_mod = @import("config.zig");

const NssStatus = types.NssStatus;

/// C group structure
pub const Group = extern struct {
    gr_name: ?[*:0]u8,
    gr_passwd: ?[*:0]u8,
    gr_gid: u32,
    gr_mem: ?[*]?[*:0]u8,
};

/// Static strings
const passwd_str = "x";

/// Pack group data into group struct
pub fn packGroupStruct(
    users: []const types.User,
    config: *const config_mod.Config,
    result: *Group,
    buffer: [*]u8,
    buflen: usize,
) NssStatus {
    var offset: usize = 0;
    const allocator = common.nss_allocator;

    // gr_name
    const group_name = config.group_name orelse "octopass";
    if (offset + group_name.len + 1 > buflen) return .tryagain;
    result.gr_name = common.copyToBuffer(buffer + offset, buflen - offset, group_name);
    if (result.gr_name == null) return .tryagain;
    offset += group_name.len + 1;

    // gr_passwd (static "x")
    result.gr_passwd = @constCast(@ptrCast(passwd_str.ptr));

    // gr_gid
    result.gr_gid = @intCast(config.gid);

    // gr_mem (array of member names)
    const mem_array = allocator.alloc(?[*:0]u8, users.len + 1) catch {
        return .tryagain;
    };

    for (users, 0..) |user, i| {
        const login = user.login;
        if (offset + login.len + 1 > buflen) {
            allocator.free(mem_array);
            return .tryagain;
        }
        mem_array[i] = common.copyToBuffer(buffer + offset, buflen - offset, login);
        if (mem_array[i] == null) {
            allocator.free(mem_array);
            return .tryagain;
        }
        offset += login.len + 1;
    }
    mem_array[users.len] = null;

    result.gr_mem = mem_array.ptr;

    return .success;
}

/// setgrent - Initialize group enumeration
pub fn setgrent(stayopen: c_int) NssStatus {
    common.nss_mutex.lock();
    defer common.nss_mutex.unlock();

    _ = stayopen;

    const state = common.getGlobalState();

    state.loadMembers() catch |err| {
        state.logger.err("setgrent failed: {}", .{err});
        return .unavail;
    };

    state.index = 0;
    return .success;
}

/// endgrent - End group enumeration
pub fn endgrent() NssStatus {
    common.nss_mutex.lock();
    defer common.nss_mutex.unlock();

    const state = common.getGlobalState();
    state.reset();

    return .success;
}

/// getgrent_r - Get next group entry
pub fn getgrent_r(
    result: *Group,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) NssStatus {
    common.nss_mutex.lock();
    defer common.nss_mutex.unlock();

    const state = common.getGlobalState();

    if (!state.initialized) {
        state.loadMembers() catch {
            errnop.* = @intFromEnum(std.posix.E.NOENT);
            return .unavail;
        };
    }

    const users = state.users orelse {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .unavail;
    };

    const config = state.config orelse {
        errnop.* = @intFromEnum(std.posix.E.IO);
        return .unavail;
    };

    if (state.index > 0) {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .notfound;
    }

    const status = packGroupStruct(users, &config, result, buffer, buflen);
    if (status != .success) {
        errnop.* = @intFromEnum(std.posix.E.RANGE);
        return status;
    }

    state.index += 1;
    return .success;
}

/// getgrnam_r - Find group by name
pub fn getgrnam_r(
    name: [*:0]const u8,
    result: *Group,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) NssStatus {
    common.nss_mutex.lock();
    defer common.nss_mutex.unlock();

    const state = common.getGlobalState();

    state.loadMembers() catch {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .unavail;
    };

    const users = state.users orelse {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .unavail;
    };

    const config = state.config orelse {
        errnop.* = @intFromEnum(std.posix.E.IO);
        return .unavail;
    };

    const name_slice = std.mem.span(name);
    const group_name = config.group_name orelse "octopass";

    if (!std.mem.eql(u8, name_slice, group_name)) {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .notfound;
    }

    const status = packGroupStruct(users, &config, result, buffer, buflen);
    if (status != .success) {
        errnop.* = @intFromEnum(std.posix.E.RANGE);
        return status;
    }

    return .success;
}

/// getgrgid_r - Find group by gid
pub fn getgrgid_r(
    gid: u32,
    result: *Group,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) NssStatus {
    common.nss_mutex.lock();
    defer common.nss_mutex.unlock();

    const state = common.getGlobalState();

    state.loadMembers() catch {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .unavail;
    };

    const users = state.users orelse {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .unavail;
    };

    const config = state.config orelse {
        errnop.* = @intFromEnum(std.posix.E.IO);
        return .unavail;
    };

    if (gid != @as(u32, @intCast(config.gid))) {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .notfound;
    }

    const status = packGroupStruct(users, &config, result, buffer, buflen);
    if (status != .success) {
        errnop.* = @intFromEnum(std.posix.E.RANGE);
        return status;
    }

    return .success;
}

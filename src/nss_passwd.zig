const std = @import("std");
const common = @import("nss_common.zig");
const types = @import("types.zig");
const config_mod = @import("config.zig");

const NssStatus = types.NssStatus;

/// C passwd structure
pub const Passwd = extern struct {
    pw_name: ?[*:0]u8,
    pw_passwd: ?[*:0]u8,
    pw_uid: u32,
    pw_gid: u32,
    pw_gecos: ?[*:0]u8,
    pw_dir: ?[*:0]u8,
    pw_shell: ?[*:0]u8,
};

/// Static strings for passwd fields
const passwd_str = "x";
const gecos_str = "managed by octopass";

/// Pack user data into passwd struct
pub fn packPasswdStruct(
    user: *const types.User,
    config: *const config_mod.Config,
    result: *Passwd,
    buffer: [*]u8,
    buflen: usize,
) NssStatus {
    var offset: usize = 0;

    // pw_name
    const login = user.login;
    if (offset + login.len + 1 > buflen) return .tryagain;
    result.pw_name = common.copyToBuffer(buffer + offset, buflen - offset, login);
    if (result.pw_name == null) return .tryagain;
    offset += login.len + 1;

    // pw_passwd (static "x")
    result.pw_passwd = @ptrCast(@constCast(passwd_str.ptr));

    // pw_uid and pw_gid
    result.pw_uid = @intCast(config.uid_starts + user.id);
    result.pw_gid = @intCast(config.gid);

    // pw_gecos (static)
    result.pw_gecos = @ptrCast(@constCast(gecos_str.ptr));

    // pw_dir (formatted with username)
    const home_result = common.simpleFormatHomePath(
        (buffer + offset)[0 .. buflen - offset],
        config.home,
        login,
    );
    if (home_result) |home| {
        result.pw_dir = @ptrCast(buffer + offset);
        offset += home.len + 1;
    } else {
        return .tryagain;
    }

    // pw_shell
    const shell = config.shell;
    if (offset + shell.len + 1 > buflen) return .tryagain;
    result.pw_shell = common.copyToBuffer(buffer + offset, buflen - offset, shell);
    if (result.pw_shell == null) return .tryagain;

    return .success;
}

/// setpwent - Initialize passwd enumeration
pub fn setpwent(stayopen: c_int) NssStatus {
    common.nss_mutex.lock();
    defer common.nss_mutex.unlock();

    _ = stayopen;

    const state = common.getGlobalState();

    state.loadMembers() catch |err| {
        state.logger.err("setpwent failed: {}", .{err});
        return .unavail;
    };

    state.index = 0;
    state.logger.info("setpwent: loaded {d} users", .{state.users.?.len});

    return .success;
}

/// endpwent - End passwd enumeration
pub fn endpwent() NssStatus {
    common.nss_mutex.lock();
    defer common.nss_mutex.unlock();

    const state = common.getGlobalState();
    state.reset();

    return .success;
}

/// getpwent_r - Get next passwd entry
pub fn getpwent_r(
    result: *Passwd,
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

    if (state.index >= users.len) {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .notfound;
    }

    const config = state.config orelse {
        errnop.* = @intFromEnum(std.posix.E.IO);
        return .unavail;
    };

    const status = packPasswdStruct(&users[state.index], &config, result, buffer, buflen);
    if (status != .success) {
        errnop.* = @intFromEnum(std.posix.E.RANGE);
        return status;
    }

    state.index += 1;
    return .success;
}

/// getpwnam_r - Find passwd by name
pub fn getpwnam_r(
    name: [*:0]const u8,
    result: *Passwd,
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

    const user = types.findUserByLogin(users, name_slice) orelse {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .notfound;
    };

    const status = packPasswdStruct(user, &config, result, buffer, buflen);
    if (status != .success) {
        errnop.* = @intFromEnum(std.posix.E.RANGE);
        return status;
    }

    return .success;
}

/// getpwuid_r - Find passwd by uid
pub fn getpwuid_r(
    uid: u32,
    result: *Passwd,
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

    const gh_id: i64 = @as(i64, uid) - config.uid_starts;
    if (gh_id < 0) {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .notfound;
    }

    const user = types.findUserById(users, gh_id) orelse {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .notfound;
    };

    const status = packPasswdStruct(user, &config, result, buffer, buflen);
    if (status != .success) {
        errnop.* = @intFromEnum(std.posix.E.RANGE);
        return status;
    }

    return .success;
}

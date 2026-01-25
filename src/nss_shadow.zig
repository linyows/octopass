const std = @import("std");
const common = @import("nss_common.zig");
const types = @import("types.zig");

const NssStatus = types.NssStatus;

/// C shadow structure (spwd)
pub const Shadow = extern struct {
    sp_namp: ?[*:0]u8,
    sp_pwdp: ?[*:0]u8,
    sp_lstchg: c_long,
    sp_min: c_long,
    sp_max: c_long,
    sp_warn: c_long,
    sp_inact: c_long,
    sp_expire: c_long,
    sp_flag: c_ulong,
};

/// Static strings
const locked_passwd = "!!";

/// Pack shadow data into shadow struct
pub fn packShadowStruct(
    user: *const types.User,
    result: *Shadow,
    buffer: [*]u8,
    buflen: usize,
) NssStatus {
    var offset: usize = 0;

    // sp_namp (login name)
    const login = user.login;
    if (offset + login.len + 1 > buflen) return .tryagain;
    result.sp_namp = common.copyToBuffer(buffer + offset, buflen - offset, login);
    if (result.sp_namp == null) return .tryagain;
    offset += login.len + 1;

    // sp_pwdp (locked password)
    result.sp_pwdp = @constCast(@ptrCast(locked_passwd.ptr));

    // All other fields set to -1 (unset)
    result.sp_lstchg = -1;
    result.sp_min = -1;
    result.sp_max = -1;
    result.sp_warn = -1;
    result.sp_inact = -1;
    result.sp_expire = -1;
    result.sp_flag = ~@as(c_ulong, 0);

    return .success;
}

/// setspent - Initialize shadow enumeration
pub fn setspent(stayopen: c_int) NssStatus {
    common.nss_mutex.lock();
    defer common.nss_mutex.unlock();

    _ = stayopen;

    const state = common.getGlobalState();

    state.loadMembers() catch |err| {
        state.logger.err("setspent failed: {}", .{err});
        return .unavail;
    };

    state.index = 0;
    return .success;
}

/// endspent - End shadow enumeration
pub fn endspent() NssStatus {
    common.nss_mutex.lock();
    defer common.nss_mutex.unlock();

    const state = common.getGlobalState();
    state.reset();

    return .success;
}

/// getspent_r - Get next shadow entry
pub fn getspent_r(
    result: *Shadow,
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

    const status = packShadowStruct(&users[state.index], result, buffer, buflen);
    if (status != .success) {
        errnop.* = @intFromEnum(std.posix.E.RANGE);
        return status;
    }

    state.index += 1;
    return .success;
}

/// getspnam_r - Find shadow by name
pub fn getspnam_r(
    name: [*:0]const u8,
    result: *Shadow,
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

    const name_slice = std.mem.span(name);

    const user = types.findUserByLogin(users, name_slice) orelse {
        errnop.* = @intFromEnum(std.posix.E.NOENT);
        return .notfound;
    };

    const status = packShadowStruct(user, result, buffer, buflen);
    if (status != .success) {
        errnop.* = @intFromEnum(std.posix.E.RANGE);
        return status;
    }

    return .success;
}

// NSS C ABI Exports
// This file exports all NSS functions with the required C naming convention

const passwd = @import("nss_passwd.zig");
const group = @import("nss_group.zig");
const shadow = @import("nss_shadow.zig");

// ============================================================
// passwd database functions
// ============================================================

export fn _nss_octopass_setpwent(stayopen: c_int) c_int {
    return @intFromEnum(passwd.setpwent(stayopen));
}

export fn _nss_octopass_endpwent() c_int {
    return @intFromEnum(passwd.endpwent());
}

export fn _nss_octopass_getpwent_r(
    result: *passwd.Passwd,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) c_int {
    return @intFromEnum(passwd.getpwent_r(result, buffer, buflen, errnop));
}

export fn _nss_octopass_getpwnam_r(
    name: [*:0]const u8,
    result: *passwd.Passwd,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) c_int {
    return @intFromEnum(passwd.getpwnam_r(name, result, buffer, buflen, errnop));
}

export fn _nss_octopass_getpwuid_r(
    uid: u32,
    result: *passwd.Passwd,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) c_int {
    return @intFromEnum(passwd.getpwuid_r(uid, result, buffer, buflen, errnop));
}

// ============================================================
// group database functions
// ============================================================

export fn _nss_octopass_setgrent(stayopen: c_int) c_int {
    return @intFromEnum(group.setgrent(stayopen));
}

export fn _nss_octopass_endgrent() c_int {
    return @intFromEnum(group.endgrent());
}

export fn _nss_octopass_getgrent_r(
    result: *group.Group,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) c_int {
    return @intFromEnum(group.getgrent_r(result, buffer, buflen, errnop));
}

export fn _nss_octopass_getgrnam_r(
    name: [*:0]const u8,
    result: *group.Group,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) c_int {
    return @intFromEnum(group.getgrnam_r(name, result, buffer, buflen, errnop));
}

export fn _nss_octopass_getgrgid_r(
    gid: u32,
    result: *group.Group,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) c_int {
    return @intFromEnum(group.getgrgid_r(gid, result, buffer, buflen, errnop));
}

// ============================================================
// shadow database functions
// ============================================================

export fn _nss_octopass_setspent(stayopen: c_int) c_int {
    return @intFromEnum(shadow.setspent(stayopen));
}

export fn _nss_octopass_endspent() c_int {
    return @intFromEnum(shadow.endspent());
}

export fn _nss_octopass_getspent_r(
    result: *shadow.Shadow,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) c_int {
    return @intFromEnum(shadow.getspent_r(result, buffer, buflen, errnop));
}

export fn _nss_octopass_getspnam_r(
    name: [*:0]const u8,
    result: *shadow.Shadow,
    buffer: [*]u8,
    buflen: usize,
    errnop: *c_int,
) c_int {
    return @intFromEnum(shadow.getspnam_r(name, result, buffer, buflen, errnop));
}

pub const DbHandle = usize; // pointer-sized integer
pub const INVALID_DB_HANDLE: DbHandle = 0;

pub const ErrorCode = enum(c_int) { ok = 0, invalid_argument = 1, internal_error = 2, open_fail, close_fail, preparation_fail, insertion_error, execution_fail };

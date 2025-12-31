pub const DbHandle = usize; // pointer-sized integer

pub const INVALID_DB_HANDLE: DbHandle = 0;

pub const ErrorCode = enum(c_int) {
    ok = 0,
    invalid_argument = 1,
    internal_error = 2,

    open_fail = 3,
    close_fail = 4,

    preparation_fail = 5,
    insertion_error = 6,
    execution_fail = 7,
    read_row_fail = 8,
};

//! Log codes of the Perl Gorgone, kept by the REST compatibility layer.

/// Action accepted.
pub const ACTION_BEGIN: i32 = 0;
/// Action finished with an error.
pub const ACTION_FINISH_KO: i32 = 1;
/// Action finished successfully.
pub const ACTION_FINISH_OK: i32 = 2;
/// Intermediate information.
pub const ACTION_CONTINUE: i32 = 4;
/// Result of one command of a `RunCommands` action.
pub const COMMAND_RESULT: i32 = 100;

/// True when `code` ends a job.
#[must_use]
pub fn is_terminal(code: i32) -> bool {
    code == ACTION_FINISH_KO || code == ACTION_FINISH_OK
}

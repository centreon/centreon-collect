# Ignore some signals, especially SIGTERM.
$SIG{HUP} = "IGNORE";
$SIG{ILL} = "IGNORE";
$SIG{INT} = "IGNORE";
$SIG{SEGV} = "IGNORE";
$SIG{TERM} = "IGNORE";

# Very long sleep.
sleep 42424;
exit 2;

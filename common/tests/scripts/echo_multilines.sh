#!/bin/sh
# echo_multilines.sh
# Outputs a fixed multi-line content with perfdata, matching test format.
# Exit code is always 0.

cat <<'EOF'
OK - load average: 0.00 |load1=0.000;5.000;10.000;0; load5=0.000;4.000;8.000;0; load15=0.000;3.000;6.000;0;
OK - load1
OK - load5
OK - load15
EOF

exit 0

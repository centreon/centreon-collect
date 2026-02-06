#!/bin/bash
# Simple event_script test script
# This script receives JSON data as argument and logs it

EVENT_LOG="/tmp/event_script_test.log"

# Log the event data received
echo "$(date '+%Y-%m-%d %H:%M:%S') - Event received: $1" >> "$EVENT_LOG"

# Exit successfully
exit 0

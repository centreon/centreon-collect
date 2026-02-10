rm -rf coverage
grcov . -s . --binary-path ./build/ -t html --branch --ignore-not-existing -o ./coverage/
firefox ./coverage/index.html

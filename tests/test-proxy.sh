#!/bin/bash
mkdir -p wget_results
../build/AsyncHttpProxy 5555 &
PROXY_PID=$!
sleep 2s

http_proxy="http://127.0.0.1:5555" wget -t 1 -P wget_results httpforever.com
http_proxy="http://127.0.0.1:5555" wget -t 1 -P wget_results httpforever.com:80

# WRONG PORT
http_proxy="http://127.0.0.1:8080" wget -t 1 -P wget_results httpforever.com
http_proxy="http://127.0.0.1:5555" wget -t 1 -P wget_results httpforever.com:65535

sleep 2s
kill $PROXY_PID

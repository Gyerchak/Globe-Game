#!/usr/bin/env bash
set -u
run="$1"
for i in 1 2 3 4 5 6 7 8; do
  if gh run view "$run" --log -R Gyerchak/Gyerchak > /tmp/opencode/metrics-log.txt 2>/dev/null; then
    echo OK
    exit 0
  fi
  sleep 8
done
echo FAILED
exit 1

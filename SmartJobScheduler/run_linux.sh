#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
mkdir -p data
if [ ! -x ./SmartJobScheduler ]; then
  gcc -std=c11 -O2 -Wall -Wextra src/server.c -o SmartJobScheduler -lm
fi
printf '\nSmart Job Scheduler: http://localhost:8080\nPress Ctrl+C to stop.\n\n'
./SmartJobScheduler 8080

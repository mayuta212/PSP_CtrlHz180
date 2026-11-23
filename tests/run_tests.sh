#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
for mode in 0 1; do
  cc -std=c99 -O2 -Wall -Wextra -Werror -DCTRL_DEBOUNCE="$mode" -I stubs test_host.c -o "/tmp/ctrlhz180_v15_test_$mode"
  "/tmp/ctrlhz180_v15_test_$mode"
done

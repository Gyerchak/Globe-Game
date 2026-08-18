#!/usr/bin/env bash
# tokenprice.sh -- DeepSeek peak/off-peak token price note for OpenCodeBox.
# DeepSeek (per api-docs.deepseek.com/quick_start/pricing, effective 2026-08-16):
#   peak hours   = 01:00-04:00 UTC and 06:00-10:00 UTC (full price, ~2x)
#   off-peak     = all other hours (half price)
# Prints a short human note for the current local time: is it cheap or
# expensive now, and how long until the price level changes. Optionally sends
# a desktop notification (--notify) with the same text.
#
# Usage: tokenprice.sh [--notify]

set -euo pipefail

PEAK_RANGES=("01:00-04:00" "06:00-10:00")
NOTIFY=0
[ "${1:-}" = "--notify" ] && NOTIFY=1

# Current UTC as HH:MM (24h) + minute-of-day.
utc_now=$(date -u +%H:%M)
utc_min=$(( 10#${utc_now:0:2} * 60 + 10#${utc_now:3:2} ))

# Parse each peak range into start/end minute-of-day.
peak_min=()
for r in "${PEAK_RANGES[@]}"; do
  start="${r%%-*}"; end="${r##*-}"
  peak_min+=("$(( 10#${start:0:2} * 60 + 10#${start:3:2} )) $(( 10#${end:0:2} * 60 + 10#${end:3:2} ))" )
done

# Determine current tier and next transition.
is_peak=0
next_change=1440  # worst case: a full day
for pair in "${peak_min[@]}"; do
  read -r s e <<< "$pair"
  if [ "$utc_min" -ge "$s" ] && [ "$utc_min" -lt "$e" ]; then
    is_peak=1
    d=$(( e - utc_min )); [ "$d" -lt "$next_change" ] && next_change=$d
  else
    d=$(( (s - utc_min + 1440) % 1440 ))
    [ "$d" -lt "$next_change" ] && next_change=$d
  fi
done

h=$(( next_change / 60 )); m=$(( next_change % 60 ))
if [ "$is_peak" = "1" ]; then
  level="expensive (PEAK)  -- off-peak starts in ${h}h ${m}m"
else
  level="cheap (OFF-PEAK)  -- peak starts in ${h}h ${m}m"
fi

note="DeepSeek 🐋 tokens now: $level
Peak hours (UTC): 01:00-04:00 & 06:00-10:00  |  off-peak = half price
Local time: $(date '+%H:%M %Z')"

echo "$note"
if [ "$NOTIFY" = "1" ]; then
  if command -v notify-send; then
    notify-send -a OpenCodeBox "DeepSeek token price" "$note" || true
  fi
fi

#!/bin/bash
set -euo pipefail

HDC_BIN=${HDC_BIN:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc}
BUNDLE_NAME=${BUNDLE_NAME:-com.ethelandev.myworld}
PHASE=${PHASE:-normal}
DURATION_SECONDS=${DURATION_SECONDS:-30}
OUTPUT_CSV=${OUTPUT_CSV:-/tmp/my-world-profile-${PHASE}.csv}

if [[ "$PHASE" != "normal" && "$PHASE" != "boss" ]]; then
  echo "PHASE must be normal or boss" >&2
  exit 2
fi
if [[ ! "$DURATION_SECONDS" =~ ^[0-9]+$ ]] || (( DURATION_SECONDS < 1 )); then
  echo "DURATION_SECONDS must be a positive integer" >&2
  exit 2
fi
if [[ ! -x "$HDC_BIN" ]]; then
  echo "hdc not executable: $HDC_BIN" >&2
  exit 2
fi

pid=$($HDC_BIN shell pidof "$BUNDLE_NAME" | tr -d '\r[:space:]')
if [[ -z "$pid" ]]; then
  echo "application PID not found: $BUNDLE_NAME" >&2
  exit 1
fi

log_file=$(mktemp /tmp/my-world-profile-hilog.XXXXXX)
trap 'rm -f "$log_file"' EXIT
printf '%s\n' 'timestamp,pid,fps,perf_level,environment_ready,environment_draw_calls,environment_triangles,environment_texture_tier,encounter_mode' > "$OUTPUT_CSV"

$HDC_BIN shell hilog -r >/dev/null
$HDC_BIN shell hilog > "$log_file" 2>&1 &
hilog_pid=$!
trap 'kill "$hilog_pid" 2>/dev/null || true; rm -f "$log_file"' EXIT

for ((second = 0; second < DURATION_SECONDS; ++second)); do
  sleep 1
  current_pid=$($HDC_BIN shell pidof "$BUNDLE_NAME" | tr -d '\r[:space:]')
  if [[ -z "$current_pid" || "$current_pid" != "$pid" ]]; then
    echo "application PID disappeared or changed" >&2
    exit 1
  fi
done
kill "$hilog_pid" 2>/dev/null || true
wait "$hilog_pid" 2>/dev/null || true

if grep -E 'SIGSEGV|cppcrash|glGetString.*invalid|RequestBuffer.*fail|EGL.*error' "$log_file" >/dev/null; then
  echo "fatal rendering signature found in HiLog" >&2
  grep -E 'SIGSEGV|cppcrash|glGetString.*invalid|RequestBuffer.*fail|EGL.*error' "$log_file" >&2
  exit 1
fi

awk -v pid="$pid" '
  /PROFILE fps=/ {
    fps = perf = ready = calls = triangles = tier = mode = ""
    for (i = 1; i <= NF; ++i) {
      split($i, pair, "=")
      if (pair[1] == "fps") fps = pair[2]
      else if (pair[1] == "perf_level") perf = pair[2]
      else if (pair[1] == "environment_ready") ready = pair[2]
      else if (pair[1] == "environment_draw_calls") calls = pair[2]
      else if (pair[1] == "environment_triangles") triangles = pair[2]
      else if (pair[1] == "environment_texture_tier") tier = pair[2]
      else if (pair[1] == "encounter_mode") mode = pair[2]
    }
    if (fps != "" && perf != "" && ready != "" && calls != "" &&
        triangles != "" && tier != "" && mode != "") {
      timestamp = $1 "T" $2
      print timestamp "," pid "," fps "," perf "," ready "," calls "," triangles "," tier "," mode
    }
  }
' "$log_file" >> "$OUTPUT_CSV"

sample_count=$(( $(wc -l < "$OUTPUT_CSV") - 1 ))
if (( sample_count < DURATION_SECONDS - 2 )); then
  echo "insufficient PROFILE samples: $sample_count" >&2
  exit 1
fi

threshold=30
[[ "$PHASE" == "boss" ]] && threshold=24
if awk -F, -v threshold="$threshold" '
  NR == 1 { next }
  $3 + 0 < threshold { consecutive++; if (consecutive > 2) exit 1; next }
  { consecutive = 0 }
  END { exit(consecutive > 2 ? 1 : 0) }
' "$OUTPUT_CSV"; then
  :
else
  echo "$PHASE FPS remained below $threshold for more than two seconds" >&2
  exit 1
fi

echo "profile collected: $OUTPUT_CSV ($sample_count samples)"

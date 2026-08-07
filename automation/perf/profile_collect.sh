#!/bin/bash
set -euo pipefail

HDC_BIN=${HDC_BIN:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc}
BUNDLE_NAME=${BUNDLE_NAME:-com.ethelandev.myworld}
PHASE=${PHASE:-normal}
DURATION_SECONDS=${DURATION_SECONDS:-30}
OUTPUT_CSV=${OUTPUT_CSV:-/tmp/my-world-profile-${PHASE}.csv}

if [[ "$PHASE" != "normal" && "$PHASE" != "boss" && "$PHASE" != "streaming" ]]; then
  echo "PHASE must be normal, boss or streaming" >&2
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
# CSV 表头：性能仪表扩展后追加 active_chunks/streaming_pending/wild_enemies 列，
# 保持既有列顺序与字段名不变。
printf '%s\n' 'timestamp,pid,fps,perf_level,environment_ready,environment_draw_calls,environment_triangles,environment_texture_tier,encounter_mode,active_chunks,streaming_pending,wild_enemies' > "$OUTPUT_CSV"

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
    chunks = pending = wild = ""
    for (i = 1; i <= NF; ++i) {
      split($i, pair, "=")
      if (pair[1] == "fps") fps = pair[2]
      else if (pair[1] == "perf_level") perf = pair[2]
      else if (pair[1] == "environment_ready") ready = pair[2]
      else if (pair[1] == "environment_draw_calls") calls = pair[2]
      else if (pair[1] == "environment_triangles") triangles = pair[2]
      else if (pair[1] == "environment_texture_tier") tier = pair[2]
      else if (pair[1] == "encounter_mode") mode = pair[2]
      else if (pair[1] == "active_chunks") chunks = pair[2]
      else if (pair[1] == "streaming_pending") pending = pair[2]
      else if (pair[1] == "wild_enemies") wild = pair[2]
    }
    if (fps != "" && perf != "" && ready != "" && calls != "" &&
        triangles != "" && tier != "" && mode != "" &&
        chunks != "" && pending != "" && wild != "") {
      timestamp = $1 "T" $2
      print timestamp "," pid "," fps "," perf "," ready "," calls "," triangles "," tier "," mode "," chunks "," pending "," wild
    }
  }
' "$log_file" >> "$OUTPUT_CSV"

sample_count=$(( $(wc -l < "$OUTPUT_CSV") - 1 ))
if (( sample_count < DURATION_SECONDS - 2 )); then
  echo "insufficient PROFILE samples: $sample_count" >&2
  exit 1
fi

# 帧率阈值：normal/streaming 要求 ≥30fps，boss 放宽到 ≥24fps；
# streaming 场景复用既有连续不达标逻辑（连续超过 2 秒低于阈值即失败）。
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

#!/usr/bin/env bash
# 完整宿主测试集：编译除 surface.cpp、loop.cpp 与 Harmony 平台文件外的全部
# Native 对象，逐个构建运行 tests/test_*.cpp，再单独链接运行 Loop 集成测试。
# 显式跳过 test_fence_wait（依赖 Harmony 生命周期）。任何编译失败立即停止，
# 不把未执行项记为通过。
cd "$(dirname "$0")/.."

WORK="$(mktemp -d /tmp/myworld-full-host.XXXXXX)"
OBJ="$WORK/obj"
BIN="$WORK/bin"
mkdir -p "$OBJ" "$BIN"
SDKROOT="$(xcrun --show-sdk-path)"
CXXFLAGS="-std=c++17 -pthread -isysroot $SDKROOT -isystem $SDKROOT/usr/include/c++/v1 -I. -Inative -Inative/engine/math"

HOST_SOURCES=()
while IFS= read -r source; do
  HOST_SOURCES+=("$source")
done < <(
  find native -name '*.cpp' \
    ! -path '*render/surface.cpp' \
    ! -path '*core/loop.cpp' \
    ! -path '*harmony/fence_wait.cpp' \
    ! -path '*harmony/lifecycle.cpp' | sort)

obj_for() {
  local object="$OBJ/$(printf '%s' "$1" | tr '/' '_')"
  printf '%s' "${object%.cpp}.o"
}

echo "== compiling ${#HOST_SOURCES[@]} host objects =="
compile_one() {
  local source="$1"
  local object
  object="$(obj_for "$source")"
  if ! clang++ $CXXFLAGS -c "$source" -o "$object" 2>"$object.log"; then
    echo "COMPILE_FAIL $source"
    cat "$object.log"
    return 1
  fi
}
export -f compile_one obj_for
export OBJ CXXFLAGS
printf '%s\n' "${HOST_SOURCES[@]}" | xargs -P 8 -I{} bash -c 'compile_one "$@"' _ {} || exit 1
echo "== objects ready =="

OBJECTS=()
for source in "${HOST_SOURCES[@]}"; do
  OBJECTS+=("$(obj_for "$source")")
done

LOOP_OBJECT="$OBJ/native_engine_core_loop.o"
if ! clang++ $CXXFLAGS -c native/engine/core/loop.cpp -o "$LOOP_OBJECT" \
    2>"$LOOP_OBJECT.log"; then
  echo "COMPILE_FAIL native/engine/core/loop.cpp"
  cat "$LOOP_OBJECT.log"
  exit 1
fi

pass=0
fail=0
skipped=()
for test_source in tests/test_*.cpp; do
  name="$(basename "$test_source" .cpp)"
  if [[ "$name" == "test_fence_wait" ]]; then
    skipped+=("$name (Harmony 平台依赖)")
    continue
  fi
  extra=()
  if [[ "$name" == "test_loop_integration" ]]; then
    extra+=("$LOOP_OBJECT")
  fi
  if ! clang++ $CXXFLAGS "$test_source" "${extra[@]}" "${OBJECTS[@]}" \
      -o "$BIN/$name" 2>"$BIN/$name.build.log"; then
    echo "BUILD_FAIL $name"
    tail -20 "$BIN/$name.build.log"
    exit 1
  fi
  if "$BIN/$name" >"$BIN/$name.run.log" 2>&1; then
    pass=$((pass + 1))
  else
    echo "RUN_FAIL $name"
    tail -20 "$BIN/$name.run.log"
    fail=$((fail + 1))
  fi
done

echo "== host suite: pass=$pass fail=$fail =="
if (( ${#skipped[@]} > 0 )); then
  printf 'skipped: %s\n' "${skipped[@]}"
fi
if (( fail > 0 )); then
  exit 1
fi
echo "ALL_HOST_TESTS_OK"

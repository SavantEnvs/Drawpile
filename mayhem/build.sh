#!/usr/bin/env bash
# mayhem/build.sh — Drawpile / drawdance.
#  1) sanitized build of upstream's drawdance libraries via upstream CMake (build-fuzz/)
#  2) libFuzzer + standalone binaries for each harness in mayhem/harnesses, linked against them
#  3) upstream's drawdance unit tests with normal flags (build-tests/), run by mayhem/test.sh
# Both build dirs match upstream's `/*build*/` .gitignore rule, so `git clean -ffdX` removes them.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
# SanCov edge instrumentation for the fuzzed code (the libraries and the harness objects);
# libFuzzer's runtime itself is only linked into the fuzzer binaries via $LIB_FUZZING_ENGINE.
FUZZ_COV_FLAGS="-fsanitize=fuzzer-no-link"
export CC CXX
cd "$SRC"

# Upstream server configuration: needs only Qt Core/Network/WebSockets and builds the C engine.
# CLIENT=OFF skips the desktop GUI (and libimpex); TOOLS=OFF skips the Rust CLIs.
COMMON_CMAKE=(-G Ninja -DCLIENT=OFF -DSERVER=ON -DSERVERGUI=OFF -DBUILTINSERVER=OFF -DTOOLS=OFF
              -DUSE_GENERATORS=OFF -DUPDATE_TRANSLATIONS=OFF -DINITSYS= -DINSTALL_DOC=OFF
              -DBUILD_VERSION=2.3.0 -DCMAKE_BUILD_TYPE=RelWithDebInfo)
LIBS=(dpimpex dpengine dpmsg dpcommon)
# The fuzz build uses the client configuration instead: it is the only one (without Rust) that adds
# libimpex (ORA/PSD/flat-image loaders). Only the drawdance library targets are built, not the GUI.
FUZZ_CMAKE=(-G Ninja -DCLIENT=ON -DSERVER=OFF -DSERVERGUI=OFF -DBUILTINSERVER=OFF -DTOOLS=OFF
            -DUSE_GENERATORS=OFF -DUPDATE_TRANSLATIONS=OFF -DINITSYS= -DINSTALL_DOC=OFF
            -DBUILD_VERSION=2.3.0 -DCMAKE_BUILD_TYPE=RelWithDebInfo)

# 1) sanitized libraries. RelWithDebInfo flags are overridden so NDEBUG stays off (DP_ASSERT active)
#    and the debug info is DWARF 3.
cmake -S . -B build-fuzz "${FUZZ_CMAKE[@]}" -DTESTS=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_C_FLAGS="$SANITIZER_FLAGS $FUZZ_COV_FLAGS $DEBUG_FLAGS" \
  -DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS $FUZZ_COV_FLAGS $DEBUG_FLAGS" \
  -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O1" -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O1"
cmake --build build-fuzz -j"$MAYHEM_JOBS" --target "${LIBS[@]}"

# Include dirs and defines exactly as upstream compiles dpengine (taken from compile_commands.json).
ENGINE_FLAGS=$(python3 - build-fuzz/compile_commands.json <<'PY'
import json, shlex, sys
for e in json.load(open(sys.argv[1])):
    if e["file"].endswith("libengine/dpengine/canvas_history.c"):
        args = shlex.split(e["command"])
        out, i = [], 0
        while i < len(args):
            a = args[i]
            if a in ("-I", "-isystem", "-D"):
                out += [a, args[i + 1]]; i += 2; continue
            if a.startswith(("-I", "-D", "-isystem")):
                out.append(a)
            i += 1
        print(" ".join(shlex.quote(x) for x in out)); break
PY
)
[ -n "$ENGINE_FLAGS" ] || { echo "could not read dpengine compile flags" >&2; exit 1; }
ENGINE_FLAGS="$ENGINE_FLAGS -I$SRC/src/drawdance/libimpex"
mapfile -t ARCHIVES < <(find build-fuzz -name '*.a' | sort)
LINK_LIBS=(-Wl,--start-group "${ARCHIVES[@]}" -Wl,--end-group
           $(pkg-config --libs Qt6Gui Qt6Xml Qt6Core libzip libwebpdemux libwebpmux libwebp) -lzstd -lz -lpthread -lm -ldl)

OUT="$SRC/build-fuzz/bin"
mkdir -p "$OUT"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OUT/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

build_harness() { # name source [extra cflags...]
  local name="$1" src="$2"; shift 2
  eval "set -- $ENGINE_FLAGS \"\$@\""
  $CC $SANITIZER_FLAGS $FUZZ_COV_FLAGS $DEBUG_FLAGS -O1 "$@" -c "$src" -o "$OUT/$name.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/$name.o" "$OUT/lsan_off.o" \
    "${LINK_LIBS[@]}" -o "$OUT/$name"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/standalone_main.o" "$OUT/$name.o" "$OUT/lsan_off.o" \
    "${LINK_LIBS[@]}" -o "$OUT/$name-standalone"
}
build_harness fuzz_canvas_history mayhem/harnesses/fuzz_canvas_history.c
build_harness fuzz_message_roundtrip mayhem/harnesses/fuzz_message_roundtrip.c
build_harness fuzz_player_binary mayhem/harnesses/fuzz_player.c -DFUZZ_PLAYER_TYPE=DP_PLAYER_TYPE_BINARY
build_harness fuzz_player_text mayhem/harnesses/fuzz_player.c -DFUZZ_PLAYER_TYPE=DP_PLAYER_TYPE_TEXT
build_harness fuzz_player_dump mayhem/harnesses/fuzz_player.c -DFUZZ_PLAYER_TYPE=DP_PLAYER_TYPE_DEBUG_DUMP
build_harness fuzz_project_load mayhem/harnesses/fuzz_project_load.c
build_harness fuzz_load_ora mayhem/harnesses/fuzz_load_canvas.c -DFUZZ_LOAD_TYPE=DP_SAVE_IMAGE_ORA '-DFUZZ_LOAD_SUFFIX=".ora"'
build_harness fuzz_load_guess mayhem/harnesses/fuzz_load_canvas.c -DFUZZ_LOAD_TYPE=DP_SAVE_IMAGE_UNKNOWN
build_harness fuzz_load_psd mayhem/harnesses/fuzz_load_psd.c
build_harness fuzz_image_load mayhem/harnesses/fuzz_image_load.c

# 3) upstream's drawdance unit tests (dptest, golden-file comparisons), normal flags.
cmake -S . -B build-tests "${COMMON_CMAKE[@]}" -DTESTS=ON \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build-tests -j"$MAYHEM_JOBS" --target \
  dpcommon_base64_test dpcommon_file_test dpcommon_queue_test dpcommon_rect_test dpcommon_vector_test \
  dpmsg_protover_test dpmsg_read_write_roundtrip_test \
  dpengine_handle_annotations_test dpengine_handle_layers_test dpengine_handle_metadata_test \
  dpengine_handle_timeline_test dpengine_pixel_blending_test dpengine_pixel_conversion_test \
  dpengine_project_test

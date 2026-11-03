#!/usr/bin/env bash
# Builds restFeasibilityCore for each target this Mac can build, and records each
# result. Linux and embedded Linux need a Linux host: see their commands below.
set -uo pipefail

sourceDirectory=$(cd "$(dirname "$0")" && pwd)
buildRoot=${1:?usage: buildMatrix.sh buildRoot [androidNdk] [existingDepsDirectory]}
androidNdk=${2:-$HOME/Library/Android/sdk/ndk/29.0.13599879}
depsDirectory=${3:-}
overrides=()
if [ -n "$depsDirectory" ]; then
    overrides=(-DFETCHCONTENT_SOURCE_DIR_CPPHTTPLIB="$depsDirectory/cpphttplib-src" -DFETCHCONTENT_SOURCE_DIR_MBEDTLSSOURCE="$depsDirectory/mbedtlssource-src")
fi
opensslRoot=$(brew --prefix openssl@3 2>/dev/null || true)
failures=0

buildTarget() {
    local name=$1
    shift
    local directory="$buildRoot/$name"
    local started=$SECONDS
    if cmake -S "$sourceDirectory" -B "$directory" -G Ninja -DCMAKE_BUILD_TYPE=Release "${overrides[@]}" "$@" >"$directory.configure.log" 2>&1 &&
        cmake --build "$directory" --target restFeasibilityCore >"$directory.build.log" 2>&1; then
        printf 'PASS build %s seconds=%d\n' "$name" $((SECONDS - started))
    else
        printf 'FAIL build %s : see %s.*.log\n' "$name" "$directory"
        failures=$((failures + 1))
    fi
}

mkdir -p "$buildRoot"

library=(-DREST_FEASIBILITY_LIBRARY_ONLY=ON)
mbedtls=(-DREST_FEASIBILITY_TLS_BACKEND=MBEDTLS)

buildTarget macos-arm64-openssl "${library[@]}" -DREST_FEASIBILITY_TLS_BACKEND=OPENSSL -DOPENSSL_ROOT_DIR="$opensslRoot" -DCMAKE_OSX_ARCHITECTURES=arm64
buildTarget macos-arm64-mbedtls "${library[@]}" "${mbedtls[@]}" -DCMAKE_OSX_ARCHITECTURES=arm64
buildTarget macos-x86_64-mbedtls "${library[@]}" "${mbedtls[@]}" -DCMAKE_OSX_ARCHITECTURES=x86_64

androidToolchain="$androidNdk/build/cmake/android.toolchain.cmake"
buildTarget android-arm64-mbedtls "${library[@]}" "${mbedtls[@]}" -DCMAKE_TOOLCHAIN_FILE="$androidToolchain" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29
buildTarget android-x86_64-mbedtls "${library[@]}" "${mbedtls[@]}" -DCMAKE_TOOLCHAIN_FILE="$androidToolchain" -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-29
buildTarget android-armv7-mbedtls "${library[@]}" "${mbedtls[@]}" -DCMAKE_TOOLCHAIN_FILE="$androidToolchain" -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-29

ios=(-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY)
buildTarget ios-arm64-mbedtls "${library[@]}" "${mbedtls[@]}" "${ios[@]}" -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64
buildTarget iossimulator-arm64-mbedtls "${library[@]}" "${mbedtls[@]}" "${ios[@]}" -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64
buildTarget iossimulator-x86_64-mbedtls "${library[@]}" "${mbedtls[@]}" "${ios[@]}" -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=x86_64

cat <<'EOF'
UNVERIFIED linux-x86_64-openssl : cmake -S . -B b -G Ninja -DCMAKE_BUILD_TYPE=Release -DREST_FEASIBILITY_TLS_BACKEND=OPENSSL && cmake --build b
UNVERIFIED linux-arm64-openssl : the same command on an arm64 Linux host
UNVERIFIED linux-armv7-mbedtls : cmake -S . -B b -G Ninja -DREST_FEASIBILITY_LIBRARY_ONLY=ON -DREST_FEASIBILITY_TLS_BACKEND=MBEDTLS -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=armv7 -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc && cmake --build b
EOF
printf 'SUMMARY matrix failed=%d\n' "$failures"
exit "$failures"

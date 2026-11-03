#!/usr/bin/env bash
# Checks what happens if a TLS-enabled Mbed TLS meets Kuaru's crypto-only build.
# Inputs: the mbedcrypto library from each build tree.
set -euo pipefail

restLibrary=${1:?usage: checkMbedtlsSymbols.sh restMbedcrypto kuaruMbedcrypto}
kuaruLibrary=${2:?usage: checkMbedtlsSymbols.sh restMbedcrypto kuaruMbedcrypto}

definedSymbols() {
    nm -g --defined-only "$1" 2>/dev/null | awk 'NF >= 3 {print $NF}' | sort -u
}

scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
definedSymbols "$restLibrary" >"$scratch/rest"
definedSymbols "$kuaruLibrary" >"$scratch/kuaru"
shared=$(comm -12 "$scratch/rest" "$scratch/kuaru" | wc -l | tr -d ' ')

printf 'restSymbols=%s kuaruSymbols=%s shared=%s\n' "$(wc -l <"$scratch/rest" | tr -d ' ')" "$(wc -l <"$scratch/kuaru" | tr -d ' ')" "$shared"
comm -12 "$scratch/rest" "$scratch/kuaru" | head -5
if [ "$shared" -gt 0 ]; then
    echo 'RESULT conflict: both libraries define the same global symbols, so one process must link one Mbed TLS build'
    exit 1
fi
echo 'RESULT no shared global symbols'

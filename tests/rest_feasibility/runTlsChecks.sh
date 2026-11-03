#!/usr/bin/env bash
# Independent TLS checks: curl and openssl s_client against tlsProbe's serve modes.
set -uo pipefail

probe=${1:?usage: runTlsChecks.sh tlsProbePath certsDirectory}
certs=${2:?usage: runTlsChecks.sh tlsProbePath certsDirectory}
failures=0

report() {
    local name=$1 result=$2 detail=${3:-}
    printf '%s external %s : %s\n' "$result" "$name" "$detail"
    [[ $result == PASS ]] || failures=$((failures + 1))
}

startServer() {
    coproc SERVER { "$probe" "$certs" "$1"; }
    read -r -u "${SERVER[0]}" word port
    [[ $word == PORT ]] || { echo "server did not start"; exit 1; }
}

stopServer() {
    echo >&"${SERVER[1]}"
    wait "$SERVER_PID" 2>/dev/null
}

handshake() {
    # shellcheck disable=SC2086
    openssl s_client -connect "localhost:$port" -CAfile "$certs/ca.pem" -verify_return_error -brief $1 </dev/null 2>&1
}

startServer serve

output=$(handshake "-tls1_1 -cipher DEFAULT@SECLEVEL=0")
if grep -q 'Protocol version: TLSv1.1' <<<"$output"; then report tls11Rejected FAIL "$output"; else report tls11Rejected PASS "no TLS 1.1 session"; fi

output=$(handshake "-tls1_2")
if grep -q 'Protocol version: TLSv1.2' <<<"$output"; then report tls12Accepted PASS "$(grep -m1 'Ciphersuite' <<<"$output")"; else report tls12Accepted FAIL "$output"; fi

output=$(handshake "-tls1_3")
if grep -q 'Protocol version: TLSv1.3' <<<"$output"; then report tls13Negotiated PASS "$(grep -m1 'Ciphersuite' <<<"$output")"; else report tls13Negotiated FAIL "$output"; fi

output=$(handshake "")
report defaultNegotiation PASS "$(grep -m1 'Protocol version' <<<"$output")"

status=$(curl -s -o /dev/null -w '%{http_code}' --cacert "$certs/ca.pem" "https://localhost:$port/health")
[[ $status == 200 ]] && report curlTrusted PASS "$status" || report curlTrusted FAIL "$status"

if curl -s -o /dev/null --cacert "$certs/otherCa.pem" "https://localhost:$port/health"; then report curlUntrustedCa FAIL "connected"; else report curlUntrustedCa PASS "rejected"; fi
if curl -s -o /dev/null --cacert "$certs/ca.pem" "https://127.0.0.1:$port/health"; then report curlWrongName FAIL "connected"; else report curlWrongName PASS "rejected"; fi

status=$(curl -s -o /dev/null -w '%{http_code}' -m 5 "http://localhost:$port/health")
[[ $status != 200 ]] && report curlPlainToTlsPort PASS "status=$status" || report curlPlainToTlsPort FAIL "$status"

stopServer

startServer serveMutual
status=$(curl -s -o /dev/null -w '%{http_code}' --cacert "$certs/ca.pem" "https://localhost:$port/health")
[[ $status != 200 ]] && report mutualNoCertificate PASS "status=$status" || report mutualNoCertificate FAIL "$status"
status=$(curl -s -o /dev/null -w '%{http_code}' --cacert "$certs/ca.pem" --cert "$certs/otherClient.pem" --key "$certs/otherClient.key" "https://localhost:$port/health")
[[ $status != 200 ]] && report mutualUntrusted PASS "status=$status" || report mutualUntrusted FAIL "$status"
status=$(curl -s -o /dev/null -w '%{http_code}' --cacert "$certs/ca.pem" --cert "$certs/client.pem" --key "$certs/client.key" "https://localhost:$port/health")
[[ $status == 200 ]] && report mutualTrusted PASS "$status" || report mutualTrusted FAIL "$status"
stopServer

printf 'SUMMARY external failed=%d\n' "$failures"
exit "$failures"

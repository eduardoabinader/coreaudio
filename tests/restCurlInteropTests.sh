#!/usr/bin/env bash
# curl is an independent client: it checks the engine without using RestClient.
set -uo pipefail

probe=${1:?usage: restCurlInteropTests.sh serverTestBinary certsDirectory}
certs=${2:?usage: restCurlInteropTests.sh serverTestBinary certsDirectory}
failures=0

expect() {
    local name=$1 wanted=$2 actual=$3
    if [[ $actual == *"$wanted"* ]]; then
        printf 'ok %s\n' "$name"
    else
        printf 'FAIL %s: wanted %s, got %s\n' "$name" "$wanted" "$actual"
        failures=$((failures + 1))
    fi
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

startServer --serve
base="http://127.0.0.1:$port"
expect httpHealth '"status":"ok"' "$(curl -s "$base/core/v1/health")"
expect httpBuild 'interop' "$(curl -s "$base/core/v1/getBuildVersion")"
expect httpParameter '"id":"abc"' "$(curl -s "$base/api/v1/items/abc")"
expect httpPost '{"a":1}' "$(curl -s -X POST -H 'Content-Type: application/json' -d '{"a":1}' "$base/api/v1/echo")"
expect httpHead 200 "$(curl -s -o /dev/null -w '%{http_code}' -I "$base/core/v1/health")"
expect httpNotFound '"code":"notFound"' "$(curl -s "$base/nothing")"
expect httpMethod 405 "$(curl -s -o /dev/null -w '%{http_code}' -X PUT "$base/api/v1/echo")"
expect httpRequestId 'X-Request-Id: curl-1' "$(curl -s -D - -o /dev/null -H 'X-Request-Id: curl-1' "$base/core/v1/health")"
expect httpCorrelation 'X-Correlation-Id: flow-1' "$(curl -s -D - -o /dev/null -H 'X-Correlation-Id: flow-1' "$base/core/v1/health")"
expect httpAttack 400 "$(curl -s -o /dev/null -w '%{http_code}' --path-as-is "$base/api/v1/items/%00")"
stopServer

startServer --serveTls
base="https://localhost:$port"
expect tlsTrusted '"status":"ok"' "$(curl -s --cacert "$certs/ca.pem" "$base/core/v1/health")"
expect tlsUntrusted 000 "$(curl -s -o /dev/null -w '%{http_code}' --cacert "$certs/otherCa.pem" "$base/core/v1/health")"
expect tlsWrongName 000 "$(curl -s -o /dev/null -w '%{http_code}' --cacert "$certs/ca.pem" "https://127.0.0.1:$port/core/v1/health")"
expect tlsPlainRefused 000 "$(curl -s -o /dev/null -w '%{http_code}' -m 5 "http://localhost:$port/core/v1/health")"
stopServer

printf 'interop failures: %d\n' "$failures"
exit "$failures"

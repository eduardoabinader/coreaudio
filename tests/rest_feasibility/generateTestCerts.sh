#!/usr/bin/env bash
# Generates throwaway test certificates. Output must stay outside the repository.
set -euo pipefail

outputDirectory=${1:?usage: generateTestCerts.sh outputDirectory}
mkdir -p "$outputDirectory"
cd "$outputDirectory"

newKey() {
    openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out "$1" 2>/dev/null
}

newCa() {
    newKey "$1.key"
    openssl req -x509 -new -key "$1.key" -sha256 -days 30 -subj "/CN=$2" \
        -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign" -out "$1.pem" 2>/dev/null
}

newLeaf() {
    local name=$1 ca=$2 subjectName=$3 extensions=$4
    shift 4
    newKey "$name.key"
    openssl req -new -key "$name.key" -subj "/CN=$subjectName" -out "$name.csr" 2>/dev/null
    printf '%s\n' "$extensions" >"$name.ext"
    openssl x509 -req -in "$name.csr" -CA "$ca.pem" -CAkey "$ca.key" -CAcreateserial \
        -sha256 -extfile "$name.ext" -out "$name.pem" "$@" 2>/dev/null
    rm -f "$name.csr" "$name.ext"
}

newCa ca "rest feasibility CA"
newCa otherCa "rest feasibility other CA"
newCa instanceCa "rest feasibility instance CA"

serverUsage="basicConstraints=CA:FALSE
keyUsage=digitalSignature
extendedKeyUsage=serverAuth"
clientUsage="basicConstraints=CA:FALSE
keyUsage=digitalSignature
extendedKeyUsage=clientAuth"

# The server certificate names localhost only, so 127.0.0.1 must fail verification.
newLeaf server ca localhost "$serverUsage
subjectAltName=DNS:localhost" -days 30
newLeaf instance instanceCa localhost "$serverUsage
subjectAltName=DNS:localhost" -days 30
newLeaf expired ca localhost "$serverUsage
subjectAltName=DNS:localhost" -not_before 20200101000000Z -not_after 20200102000000Z
newLeaf client ca "rest feasibility client" "$clientUsage" -days 30
newLeaf otherClient otherCa "rest feasibility other client" "$clientUsage" -days 30

printf 'this is not a certificate\n' >garbage.pem
echo "certificates written to $PWD"

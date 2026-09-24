cmake_minimum_required(VERSION 3.20)

foreach(required IN ITEMS KUARU_BINARY KUARU_CONFIG KUARU_SAMPLES_DIR OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} must be an explicit absolute path")
    endif()
endforeach()

foreach(path IN ITEMS KUARU_BINARY KUARU_CONFIG KUARU_SAMPLES_DIR OUTPUT_DIR)
    if(NOT IS_ABSOLUTE "${${path}}")
        message(FATAL_ERROR "${path} must be absolute: ${${path}}")
    endif()
endforeach()

if(NOT EXISTS "${KUARU_BINARY}")
    message(FATAL_ERROR "Kuaru binary not found: ${KUARU_BINARY}")
endif()
if(NOT EXISTS "${KUARU_CONFIG}")
    message(FATAL_ERROR "Kuaru config not found: ${KUARU_CONFIG}")
endif()
if(NOT IS_DIRECTORY "${KUARU_SAMPLES_DIR}")
    message(FATAL_ERROR "Kuaru samples directory not found: ${KUARU_SAMPLES_DIR}")
endif()

set(fixtureDirectory "${CMAKE_CURRENT_LIST_DIR}/../tests/migration/kuaru")
set(candidateManifest "${OUTPUT_DIR}/candidate-manifest.json")
if(EXISTS "${candidateManifest}")
    message(FATAL_ERROR
        "Refusing to update an existing candidate set: ${candidateManifest}\n"
        "Choose a new OUTPUT_DIR and review differences explicitly.")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(SHA256 "${KUARU_BINARY}" binarySha256)
file(SHA256 "${KUARU_CONFIG}" configSha256)
file(SHA256 "${fixtureDirectory}/reference-plan.json" planSha256)

set(entries "")

function(captureVector id fixture blockSize)
    set(wav "${OUTPUT_DIR}/${id}.wav")
    set(raw "${OUTPUT_DIR}/${id}.f32le")
    set(log "${OUTPUT_DIR}/${id}.log")
    set(fixturePath "${fixtureDirectory}/${fixture}")
    file(SHA256 "${fixturePath}" fixtureSha256)

    execute_process(
        COMMAND
            "${KUARU_BINARY}"
            --config "${KUARU_CONFIG}"
            --samples-dir "${KUARU_SAMPLES_DIR}"
            --num-voices 8
            --sample-rate 48000
            --buffer-size "${blockSize}"
            --render-sidecar "${fixturePath}"
            --render-tail-s 2.0
            --render-seed 1592594996
            --render-out "${wav}"
            --render-raw-f32-out "${raw}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 180
    )
    file(WRITE "${log}" "${stdout}${stderr}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Kuaru reference render ${id} failed; see ${log}")
    endif()
    if(NOT EXISTS "${wav}" OR NOT EXISTS "${raw}")
        message(FATAL_ERROR "Kuaru reference render ${id} produced incomplete output")
    endif()

    file(SHA256 "${wav}" wavSha256)
    file(SHA256 "${raw}" rawSha256)
    file(SIZE "${wav}" wavBytes)
    string(APPEND entries
        "    {\"id\":\"${id}\",\"fixture\":\"${fixture}\","
        "\"fixtureSha256\":\"${fixtureSha256}\",\"sampleRate\":48000,"
        "\"channels\":2,\"blockSize\":${blockSize},"
        "\"seed\":1592594996,"
        "\"origin\":\"Kuaru pre-extraction offline renderer\","
        "\"comparison\":\"reviewed bit-exact or explicit tolerance\","
        "\"tolerance\":\"unapproved candidate\","
        "\"rawFloatSha256\":\"${rawSha256}\","
        "\"wavSha256\":\"${wavSha256}\",\"wavBytes\":${wavBytes}}")
    set(entries "${entries}" PARENT_SCOPE)
    set(${id}Sha256 "${rawSha256}" PARENT_SCOPE)
endfunction()

captureVector(transientTailBlock64 transient-tail.jsonl 64)
string(APPEND entries ",\n")
captureVector(transientTailBlock257 transient-tail.jsonl 257)
string(APPEND entries ",\n")
captureVector(liveChangesBlock7 live-changes.jsonl 7)
string(APPEND entries ",\n")
captureVector(liveChangesBlock512 live-changes.jsonl 512)
string(APPEND entries ",\n")
captureVector(invalidParameterRecovery invalid-parameter-recovery.jsonl 64)

set(repeatWav "${OUTPUT_DIR}/transientTailBlock64-repeat.wav")
set(repeatRaw "${OUTPUT_DIR}/transientTailBlock64-repeat.f32le")
set(repeatLog "${OUTPUT_DIR}/transientTailBlock64-repeat.log")
execute_process(
    COMMAND
        "${KUARU_BINARY}"
        --config "${KUARU_CONFIG}"
        --samples-dir "${KUARU_SAMPLES_DIR}"
        --num-voices 8
        --sample-rate 48000
        --buffer-size 64
        --render-sidecar "${fixtureDirectory}/transient-tail.jsonl"
        --render-tail-s 2.0
        --render-seed 1592594996
        --render-out "${repeatWav}"
        --render-raw-f32-out "${repeatRaw}"
    RESULT_VARIABLE repeatResult
    OUTPUT_VARIABLE repeatStdout
    ERROR_VARIABLE repeatStderr
    TIMEOUT 180
)
file(WRITE "${repeatLog}" "${repeatStdout}${repeatStderr}")
if(NOT repeatResult EQUAL 0 OR NOT EXISTS "${repeatWav}" OR NOT EXISTS "${repeatRaw}")
    message(FATAL_ERROR "Kuaru reset/determinism repeat failed; see ${repeatLog}")
endif()
file(SHA256 "${repeatRaw}" repeatSha256)
if(NOT repeatSha256 STREQUAL transientTailBlock64Sha256)
    message(FATAL_ERROR
        "Fresh-process determinism failed for transientTailBlock64:\n"
        "first=${transientTailBlock64Sha256}\nrepeat=${repeatSha256}")
endif()

file(WRITE "${candidateManifest}"
    "{\n"
    "  \"schemaVersion\": 1,\n"
    "  \"status\": \"candidate-not-golden\",\n"
    "  \"automaticGoldenUpdate\": false,\n"
    "  \"binarySha256\": \"${binarySha256}\",\n"
    "  \"configSha256\": \"${configSha256}\",\n"
    "  \"planSha256\": \"${planSha256}\",\n"
    "  \"resetDeterminismSha256\": \"${repeatSha256}\",\n"
    "  \"vectors\": [\n${entries}\n  ]\n"
    "}\n")

message(STATUS "Kuaru P8F reference candidates captured at ${OUTPUT_DIR}")
message(STATUS "Candidate manifest: ${candidateManifest}")

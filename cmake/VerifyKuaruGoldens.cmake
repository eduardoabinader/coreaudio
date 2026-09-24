cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED GOLDEN_DIRECTORY OR NOT IS_DIRECTORY "${GOLDEN_DIRECTORY}")
    message(FATAL_ERROR "GOLDEN_DIRECTORY must name the committed Kuaru golden directory")
endif()

function(verifyGolden filename expectedBytes expectedSha256)
    set(path "${GOLDEN_DIRECTORY}/${filename}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing Kuaru golden: ${path}")
    endif()
    file(SIZE "${path}" actualBytes)
    file(SHA256 "${path}" actualSha256)
    if(NOT actualBytes EQUAL expectedBytes)
        message(FATAL_ERROR
            "Kuaru golden size mismatch for ${filename}: expected ${expectedBytes}, got ${actualBytes}")
    endif()
    if(NOT actualSha256 STREQUAL expectedSha256)
        message(FATAL_ERROR
            "Kuaru golden hash mismatch for ${filename}:\n"
            "expected=${expectedSha256}\nactual=${actualSha256}")
    endif()
endfunction()

verifyGolden(
    transientTailBlock64.f32le 883200
    f7d62e660530749d28cbe737d8af9afc2eb802dae84de5072fddd15ecde9aaf5)
verifyGolden(
    transientTailBlock257.f32le 883200
    09d7a013529c62ca4568d126291b619244a06810d5f351c92b8fa2205affba15)
verifyGolden(
    liveChangesBlock7.f32le 1152000
    348078cfd070e4ebe18c5b721e16da9b1c542169cdb3ccc2a17d101b8c16a891)
verifyGolden(
    liveChangesBlock512.f32le 1152000
    becefdb0d00004a0523630dab0f0ae7a90b65cfa7e3697f558013c2da9a39bf9)
verifyGolden(
    invalidParameterRecovery.f32le 864000
    1e6339b212abbf9e66923cdaf429d2aaca94321f9ce1565600e1cb9bf1a2ff99)

message(STATUS "Verified 5 committed Kuaru golden vectors")

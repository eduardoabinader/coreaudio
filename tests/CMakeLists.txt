add_executable(
    giganteCoreTests
    EffectsChainTests.cpp
)

target_link_libraries(giganteCoreTests PRIVATE gigante::core)
target_compile_features(giganteCoreTests PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteCoreTests)

add_test(NAME giganteCoreTests COMMAND giganteCoreTests)

add_executable(giganteGoldenVectorTests GoldenVectorTests.cpp)
target_compile_features(giganteGoldenVectorTests PRIVATE cxx_std_17)
target_compile_definitions(
    giganteGoldenVectorTests
    PRIVATE GIGANTE_GOLDEN_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/golden/kuaru"
)
gigante_enable_project_warnings(giganteGoldenVectorTests)
add_test(NAME giganteGoldenVectorTests COMMAND giganteGoldenVectorTests)

add_test(
    NAME giganteGoldenHashVerification
    COMMAND
        ${CMAKE_COMMAND}
        -DGOLDEN_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/golden/kuaru
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/VerifyKuaruGoldens.cmake
)

add_library(gigantePublicHeaderIsolation OBJECT PublicHeaderIsolation.cpp)
target_link_libraries(gigantePublicHeaderIsolation PRIVATE gigante::core)
target_compile_features(gigantePublicHeaderIsolation PRIVATE cxx_std_17)
gigante_enable_project_warnings(gigantePublicHeaderIsolation)

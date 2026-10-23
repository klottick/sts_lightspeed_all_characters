cmake_minimum_required(VERSION 3.19)
project(
        sts_lightspeed
        VERSION 0.1
        DESCRIPTION "High Performance, rng accurate implementation of Slay the Spire with tools for tree search and ml."
        LANGUAGES CXX)

file (GLOB_RECURSE sts_lightspeed_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS  "-Wno-shift-count-overflow -O3")

add_subdirectory(json)
add_subdirectory(pybind11)

pybind11_add_module(slaythespire bindings/slaythespire.cpp bindings/bindings-util.cpp ${sts_lightspeed_SOURCES})
target_include_directories(slaythespire PUBLIC include)
target_include_directories(slaythespire PUBLIC json/single_include)
target_include_directories(slaythespire PUBLIC bindings)


add_executable(main apps/main.cpp ${sts_lightspeed_SOURCES})
target_link_directories(main PRIVATE json::nlohmann_json)
target_include_directories(main PUBLIC include)
target_include_directories(main PUBLIC json/include)

add_executable(test apps/test.cpp ${sts_lightspeed_SOURCES})
target_include_directories(test PUBLIC include)
target_link_directories(test PUBLIC json::nlohmann_json)
target_include_directories(test PUBLIC json/include)

add_executable(small-test apps/small-test.cpp bindings/bindings-util.cpp ${sts_lightspeed_SOURCES})
target_link_directories(small-test PRIVATE json::nlohmann_json)
target_include_directories(small-test PUBLIC include)
target_include_directories(small-test PUBLIC json/include)

option(BUILD_TESTING "Build report tests" ON)
if(BUILD_TESTING)
    add_library(sts-report-engine STATIC ${sts_lightspeed_SOURCES})
    target_include_directories(sts-report-engine PUBLIC include)
    target_link_libraries(sts-report-engine PUBLIC nlohmann_json::nlohmann_json)
    file(GLOB report_TEST_SOURCES CONFIGURE_DEPENDS "tests/report/*Tests.cpp")
    file(GLOB report_TEST_HELPERS CONFIGURE_DEPENDS "tests/report/EngineFixtures.cpp")
    add_executable(report-tests tests/report/ReportTestMain.cpp ${report_TEST_SOURCES} ${report_TEST_HELPERS})
    target_link_libraries(report-tests PRIVATE sts-report-engine)
    # The existing CLI executable is named `test`, which enable_testing() reserves.
    # Generate CTest's registry directly so that executable and build target remain unchanged.
    set(report_test_registry "")
    foreach(report_test_source IN LISTS report_TEST_SOURCES)
        file(READ "${report_test_source}" report_test_content)
        string(REGEX MATCHALL "REPORT_TEST\\([A-Za-z0-9_]+\\)" report_case_macros "${report_test_content}")
        foreach(report_case_macro IN LISTS report_case_macros)
            string(REGEX REPLACE "REPORT_TEST\\((.*)\\)" "\\1" report_case "${report_case_macro}")
            string(APPEND report_test_registry "add_test(report.${report_case} \"$<TARGET_FILE:report-tests>\" --case ${report_case})\n")
        endforeach()
    endforeach()
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/CTestTestfile.cmake" CONTENT "${report_test_registry}")
endif()
target_include_directories(small-test PUBLIC bindings)

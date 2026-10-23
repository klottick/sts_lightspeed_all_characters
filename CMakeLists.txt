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

add_library(sts-report-engine STATIC ${sts_lightspeed_SOURCES})
target_include_directories(sts-report-engine PUBLIC include)
target_link_libraries(sts-report-engine PUBLIC nlohmann_json::nlohmann_json)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(report_VIEWER_FILES
    report/viewer/viewer.html report/viewer/viewer.css report/viewer/model.js
    report/viewer/dom.js report/viewer/combat.js report/viewer/screens.js report/viewer/viewer.js)
list(TRANSFORM report_VIEWER_FILES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
set(report_RESOURCE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/report-generated/ReportResources.h")
add_custom_command(OUTPUT "${report_RESOURCE_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_report_resources.py"
        --source-dir "${CMAKE_CURRENT_SOURCE_DIR}/report/viewer" --output "${report_RESOURCE_HEADER}"
    DEPENDS ${report_VIEWER_FILES} tools/embed_report_resources.py VERBATIM)
file(GLOB report_SUPPORT_SOURCES CONFIGURE_DEPENDS "report/*.cpp")
add_library(sts-report-support STATIC ${report_SUPPORT_SOURCES} "${report_RESOURCE_HEADER}")
target_include_directories(sts-report-support PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/report-generated")
target_link_libraries(sts-report-support PUBLIC sts-report-engine)

add_executable(sts-report apps/report.cpp)
target_link_libraries(sts-report PRIVATE sts-report-support)

option(BUILD_TESTING "Build report tests" ON)
if(BUILD_TESTING)
    add_executable(report-fixtures tests/report/GenerateFixtures.cpp tests/report/EngineFixtures.cpp)
    target_link_libraries(report-fixtures PRIVATE sts-report-support)
    add_executable(report-benchmark tests/report/BenchmarkReport.cpp)
    target_link_libraries(report-benchmark PRIVATE sts-report-engine)
    file(GLOB report_TEST_SOURCES CONFIGURE_DEPENDS "tests/report/*Tests.cpp")
    file(GLOB report_TEST_HELPERS CONFIGURE_DEPENDS "tests/report/EngineFixtures.cpp")
    add_executable(report-tests tests/report/ReportTestMain.cpp ${report_TEST_SOURCES} ${report_TEST_HELPERS})
    target_link_libraries(report-tests PRIVATE sts-report-support)
    # The existing CLI executable is named `test`, which enable_testing() reserves.
    # Generate CTest's registry directly so that executable and build target remain unchanged.
    set(report_test_registry "")
    foreach(report_test_source IN LISTS report_TEST_SOURCES)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${report_test_source}")
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

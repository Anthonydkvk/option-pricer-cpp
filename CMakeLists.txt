cmake_minimum_required(VERSION 3.20)

project(option_pricer
    VERSION 0.1.0
    DESCRIPTION "Option pricer: Black-Scholes, CRR binomial tree, Monte Carlo"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Standard C++17 strict (pas d'extensions GNU) + compile_commands.json pour l'IDE
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Release par défaut : les temps de calcul mesurés doivent être réalistes
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Warnings stricts, appliqués uniquement à NOS cibles (pas à GoogleTest)
# ---------------------------------------------------------------------------
add_library(pricer_warnings INTERFACE)
if(MSVC)
    target_compile_options(pricer_warnings INTERFACE /W4 /WX)
else()
    target_compile_options(pricer_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

# ---------------------------------------------------------------------------
# Bibliothèque de pricing : tout le code financier, réutilisé par le CLI et les tests
# ---------------------------------------------------------------------------
add_library(pricer
    src/version.cpp
    src/option.cpp
    src/normal.cpp
    src/black_scholes.cpp
    src/binomial.cpp
    src/monte_carlo.cpp
    src/greeks.cpp
)
target_include_directories(pricer PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(pricer PRIVATE pricer_warnings)

# ---------------------------------------------------------------------------
# Démo en ligne de commande -> build/apps/pricer_cli
# La logique (arguments, calcul, affichage) est dans une petite bibliothèque séparée
# pour pouvoir être testée ; l'exécutable ne contient que main().
# ---------------------------------------------------------------------------
add_library(pricer_cli_core apps/cli.cpp)
target_include_directories(pricer_cli_core PUBLIC ${PROJECT_SOURCE_DIR}/apps)
target_link_libraries(pricer_cli_core PUBLIC pricer PRIVATE pricer_warnings)

add_executable(pricer_cli apps/pricer_cli.cpp)
target_link_libraries(pricer_cli PRIVATE pricer_cli_core pricer_warnings)
set_target_properties(pricer_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps)

# ---------------------------------------------------------------------------
# Tests : GoogleTest récupéré via FetchContent, lancés par CTest
# ---------------------------------------------------------------------------
option(PRICER_BUILD_TESTS "Build the unit tests" ON)

if(PRICER_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)            # on n'utilise pas gMock
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # Windows/MSVC uniquement
    FetchContent_MakeAvailable(googletest)

    add_executable(pricer_tests
        tests/test_sanity.cpp
        tests/test_option.cpp
        tests/test_normal.cpp
        tests/test_black_scholes.cpp
        tests/test_binomial.cpp
        tests/test_monte_carlo.cpp
        tests/test_greeks.cpp
        tests/test_cli.cpp
    )
    target_link_libraries(pricer_tests PRIVATE pricer_cli_core pricer_warnings
                          GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(pricer_tests)

    # Tests « de bout en bout » : on lance le vrai exécutable.
    add_test(NAME cli_table COMMAND pricer_cli --steps 201 --paths 20000)
    add_test(NAME cli_csv COMMAND pricer_cli --csv --steps 201 --paths 20000)
    set_tests_properties(cli_csv PROPERTIES
        PASS_REGULAR_EXPRESSION "^method,price,std_error")
    add_test(NAME cli_rejects_bad_argument COMMAND pricer_cli --sigma abc)
    set_tests_properties(cli_rejects_bad_argument PROPERTIES WILL_FAIL TRUE)
endif()

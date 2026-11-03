# Builds the engine-agnostic core as a static library and runs its tests with CTest.
cmake_minimum_required(VERSION 3.25)
project(WeltallTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(CORE_DIR "${REPO_ROOT}/Source/WeltallCore")
set(CATCH2_DIR "${REPO_ROOT}/ThirdParty/Catch2")

# The same constraints as Unreal's build: no exceptions, no RTTI, and warnings are errors.
if(MSVC)
	# CMake adds /EHsc by default; drop it rather than overriding it per target.
	string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	set(WELTALL_LANGUAGE_OPTIONS /EHs-c- /GR- /utf-8 /permissive-)
	set(WELTALL_LANGUAGE_DEFINITIONS _HAS_EXCEPTIONS=0)
	set(WELTALL_WARNINGS /W4 /WX)
else()
	set(WELTALL_LANGUAGE_OPTIONS -fno-exceptions -fno-rtti)
	set(WELTALL_LANGUAGE_DEFINITIONS "")
	set(WELTALL_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Werror)
endif()

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "${CORE_DIR}/Private/*.cpp")
add_library(WeltallCore STATIC ${CORE_SOURCES})
# As in Unreal's build, the module's own sources also see its private headers.
target_include_directories(WeltallCore PUBLIC "${CORE_DIR}/Public" PRIVATE "${CORE_DIR}/Private")
target_compile_options(WeltallCore PUBLIC ${WELTALL_LANGUAGE_OPTIONS} PRIVATE ${WELTALL_WARNINGS})
# Unreal's build exports the core's API from its module; built on its own, the core exports nothing.
target_compile_definitions(WeltallCore PUBLIC ${WELTALL_LANGUAGE_DEFINITIONS} WELTALLCORE_API=)

add_library(Catch2 STATIC "${CATCH2_DIR}/catch_amalgamated.cpp")
target_include_directories(Catch2 SYSTEM PUBLIC "${CATCH2_DIR}")
target_compile_options(Catch2 PRIVATE ${WELTALL_LANGUAGE_OPTIONS})
target_compile_definitions(Catch2 PRIVATE ${WELTALL_LANGUAGE_DEFINITIONS})

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/WeltallCore/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Support/*.cpp")
add_executable(WeltallCoreTests ${TEST_SOURCES})
target_include_directories(WeltallCoreTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(WeltallCoreTests PRIVATE WeltallCore Catch2)
target_compile_options(WeltallCoreTests PRIVATE ${WELTALL_WARNINGS})
target_compile_definitions(WeltallCoreTests PRIVATE WELTALL_GAMES_DIR="${REPO_ROOT}/Games" WELTALL_STORY_DIR="${REPO_ROOT}/Content/Story"
	WELTALL_EXAMPLES_DIR="${REPO_ROOT}/Content/Examples")

enable_testing()
include("${CATCH2_DIR}/Catch.cmake")
catch_discover_tests(WeltallCoreTests DISCOVERY_MODE PRE_TEST)

# Prints a disc map as the text a story document that makes a map of its own starts from (Tools/MapText.cpp).
add_executable(WeltallMapText "${CMAKE_CURRENT_SOURCE_DIR}/Tools/MapText.cpp")
target_link_libraries(WeltallMapText PRIVATE WeltallCore)
target_compile_options(WeltallMapText PRIVATE ${WELTALL_WARNINGS})
target_compile_definitions(WeltallMapText PRIVATE WELTALL_GAMES_DIR="${REPO_ROOT}/Games")

# Finds where the drawing order and the depth disagree, on the field, the world map and in battle (Tools/SortingAudit.cpp).
file(GLOB SORTING_AUDIT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tools/SortingAudit*.cpp")
add_executable(WeltallSortingAudit ${SORTING_AUDIT_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/Support/PlayedGame.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Support/SortingAudit.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/Support/SortingDrawing.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Support/WorldMapSortingAudit.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/Support/BattleSortingAudit.cpp")
target_include_directories(WeltallSortingAudit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(WeltallSortingAudit PRIVATE WeltallCore)
target_compile_options(WeltallSortingAudit PRIVATE ${WELTALL_WARNINGS})
target_compile_definitions(WeltallSortingAudit PRIVATE WELTALL_GAMES_DIR="${REPO_ROOT}/Games")

# Plays the game by machine, every map's scenes or on from a new game and saves, to find where it stops (Tools/StorySweep.cpp).
file(GLOB STORY_SWEEP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tools/StorySweep*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Support/StorySweep*.cpp")
add_executable(WeltallStorySweep ${STORY_SWEEP_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/Support/PlayedGame.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Support/StoryValues.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/Support/XorShift32.cpp")
target_include_directories(WeltallStorySweep PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(WeltallStorySweep PRIVATE WeltallCore)
target_compile_options(WeltallStorySweep PRIVATE ${WELTALL_WARNINGS})
target_compile_definitions(WeltallStorySweep PRIVATE WELTALL_GAMES_DIR="${REPO_ROOT}/Games" WELTALL_STORY_DIR="${REPO_ROOT}/Content/Story"
	WELTALL_EXAMPLES_DIR="${REPO_ROOT}/Content/Examples")


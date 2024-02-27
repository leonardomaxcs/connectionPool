# CMakeList.txt : CMake project for connectionPool, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("connectionPool")

# Add source to this project's executable.
add_executable (connectionPool "connectionPool.cpp" "connectionPool.hpp" "protocols/connection_handler.hpp" "threadpool/threadpool.hpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET connectionPool PROPERTY CXX_STANDARD 20)
endif()

find_package(libssh CONFIG REQUIRED)
target_link_libraries(connectionPool PRIVATE ssh)

find_package(Libssh2 CONFIG REQUIRED)
target_link_libraries(connectionPool PRIVATE $<IF:$<TARGET_EXISTS:Libssh2::libssh2_shared>,Libssh2::libssh2_shared,Libssh2::libssh2_static>)

# TODO: Add tests and install targets if needed.

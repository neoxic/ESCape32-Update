cmake_minimum_required(VERSION 3.15)
project(ESCape32-Update)
set(CMAKE_C_FLAGS "-O2")
add_compile_options(-Wall -Wextra -Wpedantic -Wundef -Wshadow -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes
	-Wno-variadic-macros -Wno-unused-result -Wno-unused-parameter)
file(GLOB srcs src/*.c)
add_executable(escape32-update ${srcs})
include(GNUInstallDirs)
install(TARGETS escape32-update DESTINATION ${CMAKE_INSTALL_BINDIR})

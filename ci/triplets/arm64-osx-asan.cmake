set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)

set(VCPKG_C_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all")
set(VCPKG_CXX_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all")
set(VCPKG_LINKER_FLAGS "-fsanitize=address,undefined")

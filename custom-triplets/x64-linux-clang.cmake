# Same as x64-linux.cmake, but the dependencies are built with clang
# (cmake-vcpkg.sh -clang). A clang build must not link gcc-built dependencies:
# abseil lts_20260107 PolicyFunctions tables are zero-initialized by clang and
# filled at run time by gcc code, so cbd crashes in protobuf reflection.
# The compiler is fixed here and not by the CC/CXX of the shell: ninja re-runs
# cmake (and vcpkg) without them, vcpkg would then rebuild with gcc.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)

set(ENV{CC} clang)
set(ENV{CXX} clang++)

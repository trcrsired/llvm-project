// RUN: %clangxx --target=x86_64-unknown-windows-msvc -### --sysroot=%S -fuse-ld=lld %s 2>&1 | FileCheck --check-prefix=COMPILE %s
// COMPILE: clang{{.*}}" "-cc1"
// COMPILE: "-isysroot" "[[SYSROOT:[^"]+]]"
// COMPILE: "-internal-isystem" "[[SYSROOT:[^"]+]]/include/x86_64-unknown-windows-msvc/c++/stl"
// COMPILE: "-internal-isystem" "[[SYSROOT:[^"]+]]/include/c++/stl"

// RUN: %clangxx --target=aarch64-unknown-windows-msvc -### --sysroot=%S -fuse-ld=lld %s 2>&1 | FileCheck --check-prefix=COMPILE %s
// COMPILE: clang{{.*}}" "-cc1"
// COMPILE: "-isysroot" "[[SYSROOT:[^"]+]]"
// COMPILE: "-internal-isystem" "[[SYSROOT:[^"]+]]/include/aarch64-unknown-windows-msvc/c++/stl"
// COMPILE: "-internal-isystem" "[[SYSROOT:[^"]+]]/include/c++/stl"

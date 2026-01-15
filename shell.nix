{pkgs ? import <nixpkgs> {}}:
pkgs.mkShell {
  nativeBuildInputs = [
    pkgs.gdb
    pkgs.clang-tools
    pkgs.cppcheck

    # Build tooling
    pkgs.perl
    pkgs.gnumake
  ];

  shellHook = ''
    echo ""
    echo "  Usage: ./build.pl <command>"
    echo ""
    echo "  Commands:"
    echo "    all       Build all targets (marker, iris, quickie)"
    echo "    install   Install binaries and libraries"
    echo "    uninstall Remove installed files"
    echo "    test      Run marker tests"
    echo "    lint      Run linters (clang-tidy, cppcheck)"
    echo "    format    Format code with clang-format"
    echo "    clean     Remove build artifacts"
    echo ""
    echo "  Environment variables:"
    echo "    CC, CFLAGS, PREFIX, CLANG_TIDY, CPPCHECK, CLANG_FORMAT, MAKE"
    echo ""
  '';
}

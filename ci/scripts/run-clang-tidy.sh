
# Adapted from
# https://github.com/apache/arrow-nanoarrow/blob/50336ae4d1d6cd98f0a42b32cbee77a46fed02bd/ci/scripts/run-clang-tidy.sh
set -e

main() {
    if [ $# -ne 2 ]; then
      echo "Usage: $0 <source_dir> <build_dir>"
      exit 1
    fi

    local -r source_dir="${1}"
    local -r build_dir="${2}"

    # Homebrew LLVM on macOS provides clang-tidy and run-clang-tidy
    if [ -d "/opt/homebrew/opt/llvm/bin" ]; then
      export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
    fi

    if ! command -v clang-tidy &> /dev/null; then
      echo "Error: clang-tidy not found in PATH"
      exit 1
    fi

    if ! command -v run-clang-tidy &> /dev/null; then
      echo "Error: run-clang-tidy not found in PATH"
      exit 1
    fi

    if [ $(uname) = "Darwin" ]; then
      local -r jobs=$(sysctl -n hw.ncpu)
      local -r extra_args="-extra-arg=-Wno-unknown-warning-option -extra-arg=-isysroot -extra-arg=$(xcrun --show-sdk-path)"
    else
      local -r jobs=$(nproc)
      local -r extra_args="-extra-arg=-Wno-unknown-warning-option"
    fi

    set -x

    run-clang-tidy -p "${build_dir}" -j$jobs \
        $extra_args | \
        tee "${build_dir}/clang-tidy-output.txt"

    if grep -i -e "warning:" -e "error:" -e "unable to run clang-tidy" "${build_dir}/clang-tidy-output.txt"; then
      echo "Warnings or errors found!"
      exit 1
    else
      echo "No warnings or errors found!"
    fi

    set +x
}

main "$@"

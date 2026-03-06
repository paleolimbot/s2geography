
main() {
  local -r repo_url="https://github.com/apache/arrow-nanoarrow"
  # Check releases page: https://github.com/apache/arrow-nanoarrow/releases/
  local -r commit_sha=df487811eac5bf0e6f8ee75ed497bb2a6d04bb0e

  echo "Fetching $commit_sha from $repo_url"
  SCRATCH=$(mktemp -d)
  trap 'rm -rf "$SCRATCH"' EXIT

  local -r tarball="$SCRATCH/nanoarrow.tar.gz"
  wget -O "$tarball" "$repo_url/archive/$commit_sha.tar.gz"
  tar --strip-components 1 -C "$SCRATCH" -xf "$tarball"

  # Remove previous bundle
  rm -rf src/vendored/nanoarrow

  # Build the bundle
  python "${SCRATCH}/ci/scripts/bundle.py" \
      --include-output-dir=src/vendored \
      --source-output-dir=src/vendored/nanoarrow
}

main

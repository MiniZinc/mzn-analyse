#!/usr/bin/env bash
# Fetch what mzn-analyse builds against: the libminizinc SDK and Gecode.
#
# Usage: fetch_minizinc.sh <triple>
#
# Release selection: a tagged build uses the libminizinc release with the same
# tag, anything else uses `edge` (republished on every libminizinc develop push).
# Override with MZN_RELEASE.
#
# `minizinc-compiler-only` is the full install tree, headers and CMake package
# included. Gecode and CBC come from that release's vendor.lock: libminizinc's
# exported package does find_dependency on both, so they have to be the versions
# it was built against.
#
# Requires: gh (via $GH_TOKEN), tar.
set -euxo pipefail

TRIPLE="${1:?usage: fetch_minizinc.sh <triple>}"
MZN_REPO="${MZN_REPO:-MiniZinc/libminizinc}"
VENDOR_REPO="${VENDOR_REPO:-MiniZinc/minizinc-vendor}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

if [ -n "${MZN_RELEASE:-}" ]; then
  ref="$MZN_RELEASE"
elif [ "${GITHUB_REF_TYPE:-}" = "tag" ]; then
  ref="${GITHUB_REF_NAME}"
else
  ref="edge"
fi

mkdir -p minizinc vendor
gh release download "$ref" --repo "$MZN_REPO" \
  --pattern "minizinc-compiler-only-${TRIPLE}.tar.gz" --dir . --clobber
tar -xzf "minizinc-compiler-only-${TRIPLE}.tar.gz" -C minizinc
rm -f "minizinc-compiler-only-${TRIPLE}.tar.gz"

gh release download "$ref" --repo "$MZN_REPO" --pattern "vendor.lock" --dir . --clobber

deps=gecode
case "$TRIPLE" in aarch64-windows) ;; *) deps="$deps cbc" ;; esac

for dep in $deps; do
  ver="$(grep -E "^${dep}=" vendor.lock | head -1 | cut -d= -f2-)"
  [ -n "$ver" ] || { echo "no version pinned for '$dep' in vendor.lock" >&2; exit 1; }
  asset="${dep}-${ver}-${TRIPLE}.tar.gz"
  gh release download "${dep}-${ver}" --repo "$VENDOR_REPO" --pattern "$asset" --dir . --clobber
  tar -xzf "$asset" -C vendor
  rm -f "$asset"
done

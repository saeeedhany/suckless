#!/usr/bin/env bats

load helpers/x

setup() {
  REPO="$(cd "$BATS_TEST_DIRNAME/.." && pwd)"
  WORK="$BATS_TEST_TMPDIR/note"
  rsync -a "$REPO/note/" "$WORK/"
}

@test "the text engine passes its unit tests" {
  make -C "$WORK" test
}

#!/usr/bin/env bats

setup() {
  REPO="$(cd "$BATS_TEST_DIRNAME/.." && pwd)"
  export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t
  export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1
  UP="$BATS_TEST_TMPDIR/up"
  git init -q -b master "$UP"
  printf 'one\ntwo\nthree\nfour\nfive\n' > "$UP/a.c"
  git -C "$UP" add a.c
  git -C "$UP" commit -qm v1
  git -C "$UP" tag 1.0
  ME="$BATS_TEST_TMPDIR/me"
  git init -q -b main "$ME"
  mkdir "$ME/tool"
  printf 'one\ntwo\nthree\nfour\nmine\n' > "$ME/tool/a.c"
  cp "$REPO/upstream" "$ME/upstream"
  printf 'tool %s\n' "$UP" > "$ME/upstream.list"
  git -C "$ME" add .
  git -C "$ME" commit -qm "tool with my change"
  cd "$ME"
}

upstream_release() {
  sed -i "$1" "$UP/a.c"
  git -C "$UP" commit -qam "v$2"
  git -C "$UP" tag "$2"
}

@test "track records the upstream base without touching the files" {
  run ./upstream track tool 1.0
  [ "$status" -eq 0 ]
  [ "$(cat tool/a.c)" = "$(printf 'one\ntwo\nthree\nfour\nmine')" ]
  [ -z "$(git status --porcelain)" ]
  git merge-base --is-ancestor "$(git -C "$UP" rev-parse 1.0)" HEAD
}

@test "diff shows only my changes against upstream" {
  ./upstream track tool 1.0 >/dev/null
  run ./upstream diff tool
  [ "$status" -eq 0 ]
  [[ "$output" == *"-five"* && "$output" == *"+mine"* ]]
  [ "$(grep -c '^-[^-]' <<<"$output")" -eq 1 ]
}

@test "the list names each tool's base and the newest release" {
  ./upstream track tool 1.0 >/dev/null
  upstream_release 's/one/ONE/' 2.0
  run ./upstream
  [ "$status" -eq 0 ]
  [[ "$output" == *"tool"*"1.0"*"2.0"* ]]
}

@test "merge brings in the new release and keeps my change" {
  ./upstream track tool 1.0 >/dev/null
  upstream_release 's/one/ONE/' 2.0
  run ./upstream merge tool 2.0
  [ "$status" -eq 0 ]
  [ "$(cat tool/a.c)" = "$(printf 'ONE\ntwo\nthree\nfour\nmine')" ]
  [ -z "$(git status --porcelain)" ]
  run ./upstream
  [[ "$output" == *"tool"*"2.0"*"2.0"* ]]
}

@test "a clash stops the merge and names the file" {
  ./upstream track tool 1.0 >/dev/null
  upstream_release 's/five/FIVE/' 2.0
  run ./upstream merge tool 2.0
  [ "$status" -ne 0 ]
  [[ "$output" == *"tool/a.c"* ]]
}

@test "an unknown tool is refused" {
  run ./upstream track nope 1.0
  [ "$status" -ne 0 ]
  [[ "$output" == *"usage"* || "$output" == *"nope"* ]]
}

@test "the real list names every tool with its upstream" {
  for t in dwm st dmenu dwmblocks; do
    grep -qE "^$t https?://" "$REPO/upstream.list" || { echo "missing $t"; return 1; }
  done
}

@test "add brings a new tool in from upstream and records its base" {
  printf 'tool %s\nnew %s\n' "$UP" "$UP" > upstream.list
  git commit -qam "list a new tool"
  run ./upstream add new 1.0
  [ "$status" -eq 0 ]
  [ "$(cat new/a.c)" = "$(printf 'one\ntwo\nthree\nfour\nfive')" ]
  [ -z "$(git status --porcelain)" ]
  run ./upstream
  [[ "$output" == *"new"*"1.0"* ]]
  upstream_release 's/one/ONE/' 2.0
  run ./upstream merge new 2.0
  [ "$status" -eq 0 ]
  [ "$(head -1 new/a.c)" = ONE ]
}

@test "add refuses a folder that already exists" {
  run ./upstream add tool 1.0
  [ "$status" -ne 0 ]
  [[ "$output" == *"exists"* ]]
}

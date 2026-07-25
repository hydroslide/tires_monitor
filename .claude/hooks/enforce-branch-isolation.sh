#!/usr/bin/env bash
# PreToolUse hook: block editing firmware SOURCE directly on a protected branch
# (dev / main) in the tires_monitor repo. Enforces the CLAUDE.md rule that every
# feature/bug fix is isolated on its own branch off dev before any code change.
#
# Scope is deliberately narrow so it never gets in the way:
#   - only C/C++/Arduino source files (.ino/.cpp/.cc/.cxx/.c/.h/.hpp/.hh)
#   - only inside the tires_monitor repo (checked via origin URL, works in worktrees)
#   - only when HEAD is exactly 'dev' or 'main'
# Everything else (docs, config, feature branches, other repos, scratch) is allowed.
# Fails OPEN on any uncertainty (missing jq/git, unreadable input) so it can never
# wedge an unrelated edit.
#
# Contract: exit 0 = allow; exit 2 = block, stderr shown to the model.

input="$(cat 2>/dev/null)"
command -v jq  >/dev/null 2>&1 || exit 0
command -v git >/dev/null 2>&1 || exit 0

fp="$(printf '%s' "$input" | jq -r '.tool_input.file_path // .tool_input.notebook_path // empty' 2>/dev/null)"
[ -n "$fp" ] || exit 0

case "$fp" in
  *.ino|*.cpp|*.cc|*.cxx|*.c|*.h|*.hpp|*.hh) : ;;
  *) exit 0 ;;
esac

dir="$(dirname "$fp")"
[ -d "$dir" ] || exit 0

url="$(git -C "$dir" remote get-url origin 2>/dev/null)" || exit 0
case "$url" in *tires_monitor*) : ;; *) exit 0 ;; esac

branch="$(git -C "$dir" symbolic-ref --short HEAD 2>/dev/null)" || exit 0
if [ "$branch" = "dev" ] || [ "$branch" = "main" ]; then
  cat >&2 <<EOF
BLOCKED by the branch-isolation policy: you are editing firmware source on the protected branch '$branch'.
Per this repo's CLAUDE.md, every feature/bug fix must be isolated FIRST. Do this, then retry the edit:

  git checkout dev && git pull                 # fresh base
  git checkout -b <type>/<short-desc>          # e.g. fix/menu-scroll  (then invoke the parallel-dev skill)

Alternatives: invoke the parallel-dev skill, or relaunch the session with:  claude --worktree <name>
(Docs/config are exempt — this guard only fires on .ino/.cpp/.h source in tires_monitor on dev/main.)
EOF
  exit 2
fi
exit 0

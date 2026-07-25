# Project workflow

## Branching & PR policy

**Promotion stops at `dev` for now — never PR to `main`.** `dev` is the integration / flash branch; `main` is a stable baseline and is not a merge target until we explicitly decide otherwise.

**One procedure for any feature or bug fix (i.e. firmware source work):**

1. **Isolate BEFORE editing source.** Never edit firmware source (`.ino/.cpp/.h`) while on `dev` or `main` — a `PreToolUse` hook enforces this and will block the edit (docs/config are exempt). Get isolated one of two ways:
   - launch the session with `claude --worktree <name>` (auto-isolation), **or**
   - `git checkout dev && git pull`, then `git checkout -b <type>/<slug>` (e.g. `fix/menu-scroll`, `feat/session-summary`) off the fresh `dev`, and invoke the **`parallel-dev`** skill.
2. **Work on that feature branch.** `parallel-dev` handles intra-session isolation (ephemeral `wt/*` branches, auto-escalation to a sibling worktree on conflict, strict explicit-path staging, merge-back). Use **`smart-merge`** for conflicts and **`read-parallel-sessions`** to see what other sessions are doing.
3. **Link the issue** (tracker-config below): move it to `doing`, carry `[#N]` on every commit.
4. **PR the feature branch → `dev`** when ready (never `main`); comment the issue and move it to `done`. `dev → main` is a separate, deliberate call — never automatic.
5. **Flash the device** with the latest `dev` after the feature merges. Compile-green ≠ works-on-car, so every completed feature is flashed and verified on hardware before it's considered done (`./scripts/tm.sh` handles build + flash).

**Branch naming:** feature/fix branches are `<type>/<slug>` (`feat/…`, `fix/…`, `chore/…`) off `dev`; `parallel-dev`'s ephemeral session branches are `wt/…` (never pushed). This supersedes the global `rscanlon-*` convention for this repo.

**Exempt from the isolation rule:** config/doc/tooling changes (this file, `.claude/**`, `docs/**`, `*.md`) may be committed directly to `dev` — they aren't "features," and the hook does not block them.

<!-- BEGIN tracker-config -->
## Issue tracker integration

**TRACKER_MANAGED:** true

| Setting | Value |
|---|---|
| TRACKER_TOOL | GitHub Issues |
| TRACKER_ACCESS | `gh` CLI, targeting this repo's `origin` (requires `gh auth login`) |
| TRACKER_PROJECT | `hydroslide/tires_monitor` (this repo — issues are per-repo) |
| ISSUE_KEY_PATTERN | `#[0-9]+` |
| ISSUE_URL_TEMPLATE | `https://github.com/hydroslide/tires_monitor/issues/<KEY>` |
| WORKFLOW_STATES | `backlog → todo → doing → done → deployed` (labels) |
| STATE_IN_PROGRESS | open + label `doing` (self-assign) |
| STATE_IN_REVIEW | open + label `done` (PR up, awaiting merge/deploy) |
| COMMIT_PREFIX | `[#N] ` (e.g. `[#123] feat: …`) |
| DEFAULT_PARENT | none (group with milestones/labels if desired) |
| STATE_LABELS_PROVISIONED | true — labels created (backlog/todo/doing/done/deployed) |

Story workflows are ACTIVE for this repo. Follow the agile/story instructions in the global
`~/.claude/CLAUDE.md`: link a GitHub issue before feature work, move it to `doing` and carry `[#N]`
on every commit, and at PR time comment on the issue + move it to `done`; when it ships, move it to
`deployed` and close it (`Closes #N` in the PR body auto-closes on merge). The exact `gh` calls, the
open/closed + label state model, and all field mechanics live in
`~/.claude/skills/create-story/references/tracker-conventions.md` (filled for GitHub Issues — shared
across all your GitHub repos).

**Status labels:** `backlog → todo → doing → done → deployed`. One-time per repo, after
`gh auth login`: `for l in backlog todo doing done deployed; do gh label create "$l" --force; done`.

_PR target: **`dev`** (the integration / flash branch). `main` is **not** a PR target for now — see the Branching & PR policy at the top of this file._
<!-- END tracker-config -->

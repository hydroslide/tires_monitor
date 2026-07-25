# Project workflow

## Branching & PR policy

**Promotion stops at `dev` for now — do NOT PR to `main`.** `dev` is the integration / flash branch; `main` is a stable baseline and is not a merge target until we explicitly decide otherwise.

- **Every feature or bug fix gets its own branch off a _fresh_ `dev`.** Before starting: `git checkout dev && git pull` so the branch forks from the latest `dev`.
- **Use git worktrees for safe parallel development**, per the global `~/.claude/CLAUDE.md` — invoke the `parallel-dev` skill before the first code change (branch isolation, auto-escalation to a sibling worktree on file conflict, strict explicit-path staging, merge-back cycle). Use `smart-merge` for conflicts and `read-parallel-sessions` to see what other sessions are doing.
- **PRs target `dev` only.** When a feature/fix is ready, open a PR from its branch into `dev`. Never open or merge a PR into `main`.
- Carry the issue key and run the tracker close-out at PR time per the tracker-config below.

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

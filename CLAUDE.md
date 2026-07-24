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
| STATE_LABELS_PROVISIONED | false — create the label set on first status change, then set true (conventions §3.0) |

Story workflows are ACTIVE for this repo. Follow the agile/story instructions in the global
`~/.claude/CLAUDE.md`: link a GitHub issue before feature work, move it to `doing` and carry `[#N]`
on every commit, and at PR time comment on the issue + move it to `done`; when it ships, move it to
`deployed` and close it (`Closes #N` in the PR body auto-closes on merge). The exact `gh` calls, the
open/closed + label state model, and all field mechanics live in
`~/.claude/skills/create-story/references/tracker-conventions.md` (filled for GitHub Issues — shared
across all your GitHub repos).

**Status labels:** `backlog → todo → doing → done → deployed`. One-time per repo, after
`gh auth login`: `for l in backlog todo doing done deployed; do gh label create "$l" --force; done`.

_Note: this repo has no `dev` branch — PRs will target `main` (or another branch) after the PR
workflow confirms with you; it won't assume `main` silently._
<!-- END tracker-config -->

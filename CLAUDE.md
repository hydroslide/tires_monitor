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
| STATE_IN_PROGRESS | open + label `in progress` (self-assign) |
| STATE_IN_REVIEW | open + label `in review` |
| COMMIT_PREFIX | `[#N] ` (e.g. `[#123] feat: …`) |
| DEFAULT_PARENT | none (group with milestones/labels if desired) |

Story workflows are ACTIVE for this repo. Follow the agile/story instructions in the global
`~/.claude/CLAUDE.md`: link a GitHub issue before feature work, carry `[#N]` on every commit, and
at PR time comment on the issue + move it to review (label `in review`), closing it when the PR
merges (`Closes #N` in the PR body). The exact `gh` calls, the open/closed + label state model, and
all field mechanics live in `~/.claude/skills/create-story/references/tracker-conventions.md`
(filled for GitHub Issues — shared across all your GitHub repos).

**One-time prerequisites:** run `gh auth login`, and create the two status labels in this repo:
`gh label create "in progress" --color FBCA04` and `gh label create "in review" --color 0E8A16`.

_Note: this repo has no `dev` branch — PRs will target `main` (or another branch) after the PR
workflow confirms with you; it won't assume `main` silently._
<!-- END tracker-config -->

# Project workflow

## Branching, merge & flash policy

**This is a solo repo driven by multiple parallel Claude sessions — not a team.** The isolation safeguards below are what keep those sessions from clobbering each other, so they **stay in full**. What we drop is the team-era GitHub PR gate: **a completed feature merges straight into `dev` locally — no pull request.** This section **overrides the global `~/.claude/CLAUDE.md` "Pull requests" section and every "at PR time" story step for this repo** — do not open PRs to `dev` and do not run `find-story` MODE 3 / any PR-creation flow here.

**Promotion stops at `dev` for now.** `dev` is the integration / flash branch; `main` is a stable baseline and is **not** a merge target until we explicitly decide otherwise. `dev → main` is a separate, deliberate, human call — never automatic.

**One procedure for any feature or bug fix (i.e. firmware source work):**

1. **Isolate BEFORE editing source.** Never edit firmware source (`.ino/.cpp/.h`) while on `dev` or `main` — a `PreToolUse` hook enforces this and will block the edit (docs/config are exempt). Get isolated one of two ways:
   - launch the session with `claude --worktree <name>` (auto-isolation), **or**
   - `git checkout dev && git pull`, then `git checkout -b <type>/<slug>` (e.g. `fix/menu-scroll`, `feat/session-summary`) off the fresh `dev`, and invoke the **`parallel-dev`** skill.
2. **Work on that feature branch.** `parallel-dev` handles intra-session isolation (ephemeral `wt/*` branches, auto-escalation to a sibling worktree on conflict, strict explicit-path staging, merge-back). Use **`smart-merge`** for conflicts and **`read-parallel-sessions`** to see what other sessions are doing. You may **build & flash from this branch at any time** to test the feature in isolation (`./scripts/tm.sh flash`).
3. **Link the issue** (tracker-config below): move it to `doing`, carry `[#N]` on every commit.
4. **Update the docs as part of the feature — before the flash, in the same branch and commit(s).** Documentation is part of "done," not a follow-up: we want to be able to speak to the *intent* of every feature and setting, not re-infer it from code months later.
   - **`README.md`** — update whenever the feature changes what the device does, how it is used, or how it is built/flashed. Skip only for genuinely invisible internals.
   - **`docs/SETTINGS.md`** — **mandatory** if the feature adds, removes, renames, re-scopes, or changes the default/range of **any** menu setting. One row per setting: what it is, which menu it lives in, default, range/units, **what it's for (intent)**, and how to tune it.
   - **`docs/tire-temp-functional-design.md` / `docs/stories/**`** — if the change contradicts what's written there, fix it in the same commit rather than leaving a stale claim. A doc that confidently states the old behavior is worse than no doc.
   - State in the issue close-out comment which docs you updated, or say explicitly that none applied and why.
5. **Feature done → fold `dev` in, THEN flash (in this order).** Compile-green ≠ works-on-car, so every completed feature is verified on hardware — and every flash must carry **all** the latest code so the last flash is always the newest and no feature is ever lost:
   1. `git checkout dev && git pull` — get the newest `dev` (other sessions' merged work).
   2. `git checkout <feature-branch>` and **merge `dev` into it** (`git merge dev`; resolve with **`smart-merge`**). The feature branch is now *latest `dev` + this feature*.
   3. **Flash from the feature branch** — `./scripts/tm.sh flash` — and verify on the car. This is the mandatory end-of-feature flash; it runs the union of all work.
6. **Merge the feature branch into `dev` directly (no PR), then push `dev`.** Because step 5.2 already folded `dev` in, this is normally a fast-forward, so **`dev` ends up as exactly what you just flashed.** Pushing `dev` is required — it's how the next task (and other sessions) start from a fresh, current `dev`. **This is the critical invariant: every implemented issue lands back in `dev` immediately.** Delete the merged feature branch.
7. **Close out the tracker:** comment on the issue summarizing the work, move it to `deployed`, and close it (it shipped to the device).

**Serial/parallel issue runs:** when asked to implement several issues (in sequence or in parallel), each one must complete the full step 5–6 cycle — merge back to `dev` and push — **before** the next issue branches, so every issue starts from the newest `dev` and nothing is lost.

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
| STATE_IN_REVIEW | open + label `done` (merged into `dev`, awaiting hardware flash/verify) |
| COMMIT_PREFIX | `[#N] ` (e.g. `[#123] feat: …`) |
| DEFAULT_PARENT | none (group with milestones/labels if desired) |
| STATE_LABELS_PROVISIONED | true — labels created (backlog/todo/doing/done/deployed) |

Story workflows are ACTIVE for this repo, but **there is no PR step** — the Branching, merge & flash
policy above overrides the global "at PR time" story instructions. Link a GitHub issue before feature
work, move it to `doing` and carry `[#N]` on every commit. When the feature is **merged into `dev` and
flashed/verified on hardware**, comment on the issue summarizing the work, move it to `deployed`, and
close it (`Closes #N` in a commit/merge message auto-closes the issue when `dev` is pushed). The `done`
label is the optional intermediate for "merged into `dev`, flash pending." Do **not** invoke
`find-story` MODE 3 or any PR-creation flow here. The exact `gh` calls, the open/closed + label state
model, and all field mechanics live in
`~/.claude/skills/create-story/references/tracker-conventions.md` (filled for GitHub Issues — shared
across all your GitHub repos).

**Status labels:** `backlog → todo → doing → done → deployed`. One-time per repo, after
`gh auth login`: `for l in backlog todo doing done deployed; do gh label create "$l" --force; done`.

_Merge target: **`dev`** (the integration / flash branch), via a **direct local merge — no PR** (see the
Branching, merge & flash policy at the top of this file). `main` is **not** a merge target for now._
<!-- END tracker-config -->

---
name: implement-todos
description: Autonomously implement every open GitHub issue labeled `todo` in the tires_monitor repo. First an early subagent triage (before any code is written) checks each issue is specified enough to build with no further input; under-specified issues are set aside with their dependents, or the whole run stops if the dependency graph is messy. Then it fans out independent issues in parallel using parallel-dev worktree isolation, serializes dependent/overlapping ones, and drains each finished issue through this repo's strict per-issue flash cycle (fold dev in → flash → verify on car → merge to dev → close). No PRs. Invoke when the user says "implement the todo issues", "work through the todo backlog", or runs /implement-todos.
---

# Implement Todo Issues

Read every open GitHub issue labeled **`todo`**, decide up front (via subagents, before writing any
code) which are ready to build with **no further input**, then implement them — fanning out
independent issues **in parallel** and serializing dependent/overlapping ones — and drain each
finished issue through this repo's **strict per-issue flash cycle**.

## When to invoke
- User runs `/implement-todos`, or says "implement the todo issues", "work the todo backlog",
  "build out everything in todo", or similar.

## Hard rules (read first)
- **Gate:** this repo is `TRACKER_MANAGED: true` (GitHub Issues). If that ever changes, stop.
- **No PRs, ever.** Per this repo's CLAUDE.md, a finished feature merges **directly into `dev`
  locally**. Do not open a pull request and do not run `find-story` MODE 3. `main` is not a target.
- **`[#N]` on every commit** (`[#N] feat: …` / `[#N] fix: …`), including inside worktree branches.
- **Strict staging:** `git add <explicit paths>` only — never `git add -A` / `.` / `-u` / a bare dir.
  Read `git diff --cached --name-only` before every commit.
- **"No questions asked" means implementation choices, not hardware.** Coding agents must not pester
  the user about *how* to build a clear issue. It does **not** remove the physical per-issue
  **flash + verify-on-car** checkpoint — that gate stays, because hardware can't be automated. The
  early triage is the only place the run pauses to *ask for more input*, and only per the gate below.
- **Isolation is mandatory.** Never edit firmware source (`.ino/.cpp/.h`) on `dev`/`main` — a hook
  blocks it. All coding happens in per-issue worktrees on feature branches (see `parallel-dev`).
- Use `smart-merge` for any merge conflict, and `read-parallel-sessions` if unexpected file content
  suggests another live session is touching the same code. Tracker mechanics: follow
  `~/.claude/skills/create-story/references/tracker-conventions.md` (GitHub Issues).

---

## Phase 0 — Preconditions
1. `gh auth status` succeeds (else stop: ask the user to run `gh auth login`).
2. Working tree is clean enough to start (`git status --short`). If mid-edit on unrelated work,
   stop and tell the user rather than stashing their changes.
3. Fetch the work list:
   ```bash
   gh issue list --label todo --state open --limit 100 \
     --json number,title,body,labels,url
   ```
   **If zero `todo` issues → report "No open issues in `todo`." and stop.** (Do not touch `backlog`,
   `doing`, `done`, or `deployed` issues — only `todo`.)

---

## Phase 1 — Early triage & blocker check (subagents, BEFORE any code)

This is the "check early via a subagent, before implementing anything" step. **No source is written
in this phase.**

### 1a. Fan out one triage subagent per `todo` issue (in parallel)
Spawn them in a single message (multiple `Agent` calls). Give each this brief:

> You are triaging GitHub issue **#N** in the tires_monitor ESP32 firmware repo for autonomous
> implementation. Do NOT write or edit any code.
> 1. `gh issue view N --json number,title,body,labels,comments,url` — read the full body + comments.
> 2. Read the firmware source it would touch (start in `tires_esp32/`, plus `docs/`) enough to judge
>    scope. Do not modify anything.
> 3. Decide: is this specified enough to implement end-to-end with **no further questions to the
>    user**, given the codebase? A clear acceptance-criteria list or an unambiguous change = ready.
>    Vague/contradictory scope, an undecided design choice that changes the implementation, or a
>    missing value you'd have to invent = not ready.
> Return EXACTLY this structure (JSON):
> `{ "number": N, "title": "...", "type": "feat|fix|chore",
>    "ready": true|false, "reason_if_not_ready": "... or null",
>    "declared_deps": [<issue numbers this body/comments say it depends on / is blocked by>],
>    "files_expected": ["repo-relative paths it will most likely change"],
>    "subsystem": "short tag e.g. menu|imu|session|thermal|profiles",
>    "effort": "S|M|L" }`
> `type`: derive from the issue's label — `bug` → `fix`, `enhancement` → `feat`, else `chore`.

### 1b. Synthesize the graph (main session)
From the triage results build:
- **Dependency edges:** every `declared_deps` entry (A depends on B) **plus** inferred edges where
  two issues share `files_expected` or the same `subsystem` (treat overlap as an ordering
  constraint so they never edit the same file in parallel).
- **`NOT_READY`** = issues with `ready == false`.

### 1c. Decision gate (this is the user's rule — follow it exactly)
```
if NOT_READY is empty:
    PROCEED_SET = all todo issues        # everything is clear → build it all

else:
    messy = ANY of:
        - the dependency graph has a cycle
        - a NOT_READY issue is a hub (≈3+ dependents, or blocks most of the backlog)
        - triage signals are contradictory / a NOT_READY issue can't be cleanly separated
          from the ready set (they share files with no clear ordering)
    if messy:
        STOP. Write no code. Report every NOT_READY issue with its reason and the tangle. Done.
    else:
        BLOCKED     = NOT_READY  ∪  (transitive dependents of NOT_READY)
        PROCEED_SET = all todo issues  −  BLOCKED
        Tell the user: which issues are set aside + why (the NOT_READY reasons), and which will
        be implemented now.
        if PROCEED_SET is empty:
            STOP (nothing is safe to build without input). Report. Done.
```
Mapping to the user's intent: an under-specified issue **nobody depends on** → set it aside, say so,
build the rest. If **things depend on it** → build only the issues that don't (its dependents are
blocked too). If the graph looks **messy** → just stop.

---

## Phase 2 — Schedule `PROCEED_SET` into waves
Topologically layer the graph:
- **Wave = issues whose dependencies are already merged into `dev` AND that do not share
  `files_expected`/`subsystem` with each other.** These are safe to code fully in parallel.
- Anything dependent or file-overlapping drops to a later wave.
- Cap a single wave at ~4 concurrent coding agents (parallel arduino-cli builds are heavy; more just
  thrashes the machine and burns rate limit). If a wave is larger, run it in chunks of ≤4. `log`/note
  any chunking so nothing looks silently dropped.

Announce the plan: the waves, and which issues run together vs. after which.

---

## Phase 3 — Implement one wave in parallel (parallel-dev worktrees)

Refresh `dev` once, then give every issue in the wave its own isolated worktree so the parallel
coding agents never collide on the shared working tree:
```bash
PROJECT_ROOT=$(git rev-parse --show-toplevel)
git -C "$PROJECT_ROOT" checkout dev && git -C "$PROJECT_ROOT" pull
# per issue #N (type/slug from triage; slug = 3–5 kebab words from the title):
TS=$(date +%Y%m%d-%H%M%S)
WT_DIR="$PROJECT_ROOT/../$(basename "$PROJECT_ROOT")-wt-N-$TS"
BR="<type>/<slug>"                                   # e.g. feat/session-summary, fix/menu-scroll
git -C "$PROJECT_ROOT" worktree add "$WT_DIR" -b "$BR" dev
```
Then spawn one coding `Agent` per issue **concurrently** (single message, multiple `Agent` calls),
each pinned to its `WT_DIR`. Coding-agent brief:

> Implement GitHub issue **#N** ("<title>") in the tires_monitor ESP32 firmware. Work **only**
> inside `<WT_DIR>` (absolute paths); you are on branch `<BR>`, isolated from other agents.
> 1. `gh issue edit N --add-label doing --remove-label todo --add-assignee @me`.
> 2. Implement the issue to its acceptance criteria. Match the surrounding firmware style. **Make
>    your own reasonable implementation decisions — do NOT ask the user anything.** Follow the global
>    CLAUDE.md standards.
> 3. **Compile-green gate:** `cd <WT_DIR> && ./scripts/tm.sh build` must succeed. Fix until it does.
>    (Hardware/on-car verification is a later human step — the build is your automatable gate.)
> 4. Commit on `<BR>` with strict explicit-path staging and a `[#N] <type>: <subject>` message
>    (+ a body explaining what & why). Verify `git -C <WT_DIR> diff --cached --name-only` first.
> 5. Return: `{ "number": N, "branch": "<BR>", "worktree": "<WT_DIR>", "files": [...],
>    "build_ok": true|false, "commit": "<sha>", "needs_input": false|true,
>    "note": "one-line summary, or what blocked you" }`
> If — and only if — you hit something that genuinely blocks implementation and can't be resolved by
> reasonable judgment, stop, set `needs_input: true`, explain in `note`, and do not commit.

**If any agent returns `needs_input: true` or `build_ok: false` it couldn't fix:** treat that issue
like a `NOT_READY` issue discovered late — re-run the Phase 1c gate against it (set it aside with its
dependents, or stop if that makes the remainder messy). Clean up its worktree
(`git worktree remove <WT_DIR> --force`) and continue with the rest of the wave.

---

## Phase 4 — Drain the wave (STRICT per-issue flash cycle — serial, one at a time)

For each successfully-coded issue, **in dependency order, one at a time** (this is the mandatory
serial hardware tail — only one board, only one flash at a time). All steps run inside that issue's
worktree except the final merge into `dev`:
```bash
# 1. Get the newest dev (may now contain a sibling issue drained just before this one)
git -C "$PROJECT_ROOT" checkout dev && git -C "$PROJECT_ROOT" pull

# 2. Fold dev into the feature branch, THEN it's "latest dev + this feature"
git -C "$WT_DIR" merge dev              # conflict → invoke smart-merge, then continue
cd "$WT_DIR" && ./scripts/tm.sh build   # must stay green after the merge
```
3. **Flash + verify (the per-issue human checkpoint):**
   ```bash
   cd "$WT_DIR" && ./scripts/tm.sh flash
   ```
   Then **pause and ask the user to verify on the car**: "Flashed **#N** (<title>). Verify on the
   car and reply `ok`, or describe what's wrong." Wait for the reply.
   - **Not ok →** stop draining. Report. Let the user decide (fix on the branch and re-flash, or set
     the issue aside). Do not merge a failed flash into `dev`.
4. **On `ok` — merge to `dev` (no PR), push, clean up:**
   ```bash
   git -C "$PROJECT_ROOT" merge "$BR" --no-ff -m "[#N] Merge <slug> into dev

   Closes #N"
   git -C "$PROJECT_ROOT" push origin dev          # required — next task/session starts from fresh dev
   git -C "$PROJECT_ROOT" worktree remove "$WT_DIR" --force
   git -C "$PROJECT_ROOT" branch -d "$BR"
   ```
   (Because step 2 already folded `dev` in, this is normally a fast-forward, so `dev` ends up as
   exactly what you flashed and verified — the repo's core invariant.)
5. **Close out the tracker:**
   ```bash
   gh issue comment N --body "Implemented and flashed to the device; verified on the car. <1–3 line summary of the change>."
   gh issue edit N --add-label deployed --remove-label doing
   gh issue close N --reason completed
   ```
   (The repo flashes *before* merging, so the `done` label — "merged, flash pending" — is skipped;
   go `doing` → `deployed`. The `Closes #N` in the merge message also auto-closes on push.)
6. Move to the next issue in the wave (step 1 re-pulls `dev`, so it now includes what you just shipped).

---

## Phase 5 — Next wave, then finish
After a wave fully drains, its issues are in `dev`, so their dependents are now unblocked. Recompute
the next wave (Phase 2) and repeat Phases 3–4. When no waves remain, report:
- **Shipped:** each `#N` merged to `dev` + flashed/verified + closed.
- **Set aside:** each issue skipped for needing input, with its reason (what to clarify to resume).
- **Not reached:** anything stopped on (messy graph / failed verify), with why.

---

## Reconciling "parallel" with "strict per-issue flash cycle"
Both were requested, and they compose exactly one way:
- **Coding fans out in parallel** — independent, non-overlapping issues are written simultaneously in
  separate worktrees, so all the code is ready without waiting.
- **Integration stays strictly serial** — `dev` merges and hardware flashes happen **one issue at a
  time**, each folding in the latest `dev` and verified on the car before the next. Only one board
  exists, and this preserves CLAUDE.md's invariant that every issue lands in `dev` (and is flashed)
  as a discrete, verified unit before the next.
Dependency chains never parallelize their coding: a dependent issue only enters a wave after its
prerequisite has drained into `dev`.

## References
- `parallel-dev` — worktree isolation mechanics and staging discipline.
- `smart-merge` — resolve any merge conflict during fold-in or merge-to-dev.
- `read-parallel-sessions` — if unexpected file content hints another live session is editing.
- `~/.claude/skills/create-story/references/tracker-conventions.md` — exact `gh` calls & label model.
- Project `CLAUDE.md` — the branching/merge/flash policy this skill automates (no PRs; merge to `dev`).

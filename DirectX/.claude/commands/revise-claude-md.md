description: Update CLAUDE.md with learnings from this session
allowed-tools: Read, Edit, Glob

Review this session for learnings about working with Claude Code in this codebase. Update CLAUDE.md with context that would help future Claude sessions be more effective.

## Step 1: Reflect

What context was missing that would have helped Claude work more effectively?

- Bash commands that were used or discovered
- Code style patterns followed
- Testing approaches that worked
- Environment/configuration quirks
- Warnings or gotchas encountered

## Step 2: Find CLAUDE.md Files

Search for all CLAUDE.md and .claude.local.md files in the project.

Decide where each addition belongs:
- `CLAUDE.md` - Team-shared (checked into git)
- `.claude.local.md` - Personal/local only (gitignored)

## Step 3: Check for Stale Content

Read the current CLAUDE.md and identify entries that are:
- No longer true (refactored away, renamed, deleted)
- Duplicated elsewhere
- Too specific to a one-time fix that won't recur

Flag these for removal alongside new additions.

## Step 4: Draft Changes

**Keep it concise** - one line per concept. CLAUDE.md is part of the prompt, so brevity matters.

Format: `<command or pattern>` - `<brief description>`

Avoid:
- Verbose explanations
- Obvious information
- One-off fixes unlikely to recur

## Step 5: Show Proposed Changes

Show additions and removals together as a diff before applying:

```
### Update: ./CLAUDE.md

**Why:** [one-line reason]

diff
+ [new entry]
- [stale entry being removed]
```

## Step 6: Apply with Approval

Ask if the user wants to apply the changes. Only edit files they approve.

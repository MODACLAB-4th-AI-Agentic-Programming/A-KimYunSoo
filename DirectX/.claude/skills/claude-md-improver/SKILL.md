---
name: claude-md-improver
description: Audit all CLAUDE.md files in the repository for quality and suggest targeted improvements. Use when asked to "audit my CLAUDE.md" or "check if my CLAUDE.md is up to date".
---

Audit all CLAUDE.md files in this repository and provide quality scores with improvement recommendations.

## Step 1: Discover

Find all CLAUDE.md files:
- Project root CLAUDE.md
- Package-level CLAUDE.md files
- .claude.local.md overrides
- Global ~/.claude/CLAUDE.md

## Step 2: Evaluate Quality

Score each file on these dimensions (A–F):

| Dimension | What to check |
|-----------|--------------|
| Commands | Are build/test/run commands documented? |
| Architecture | Is the project structure explained? |
| Patterns | Are non-obvious conventions captured? |
| Conciseness | Is it free of verbose or obvious content? |
| Freshness | Does it reflect the current codebase? |
| Actionability | Can Claude follow these instructions without guessing? |

## Step 3: Quality Report

For each file, output:
```
### <filepath>
Score: B+
Issues:
- Missing: build command
- Outdated: references old folder structure
- Verbose: section X can be condensed
```

## Step 4: Propose Updates

For each issue, draft a specific fix:

```
### Proposed update: CLAUDE.md

**Why:** build command not documented

diff
+ ## Commands
+ - Build: MSBuild DirectX.sln /p:Configuration=Debug /p:Platform=x64
```

## Step 5: Apply with Approval

Present all proposed changes to the user. Apply only the ones they approve using the Edit tool.

## Good CLAUDE.md Sections

- **Commands** — build, test, run, lint commands
- **Architecture** — folder layout, key files, module boundaries  
- **Code Style** — naming conventions, patterns to follow
- **Environment** — OS, toolchain, dependencies
- **Gotchas** — known pitfalls, non-obvious behaviors
- **Workflow** — how to develop/debug in this project

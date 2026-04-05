# Standards and guidelines


## Merge a PR

Before merging a PR back into main ask the 🐰 the following:

@coderabbitai, I am about to merge this PR. Please produce three outputs:

1. **PR review** — in-depth review of all commits: a concise summary of what changed and why, a merge recommendation, and a prioritised list of follow-up actions. For the most urgent items (blockers or high-risk changes), include a ready-to-paste prompt that a Claude Code agent can execute immediately before merge.

2. **End-user docs prompt** — a ready-to-paste prompt for a Claude Code agent to update `/docs/endusers`. Rules: only describe usage implications (what changed for the user); no internals, no code, no architecture; check existing pages before adding — update in place rather than duplicating; keep additions compact and user-friendly.

3. **Developer docs prompt** — a ready-to-paste prompt for a Claude Code agent to update `/docs/developers`. Rules: target contributors, not end users; be concise — if the detail is already in the code or commit messages, do not repeat it; focus on decisions, patterns, and guidance that are not obvious from reading the source.


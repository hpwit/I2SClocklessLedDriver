# Standards and guidelines

## Pull Requests (PR)

### Create a PR

Make all PRs against the `main` branch: Create a branch from the main branch first.

Please add a description of your proposed code changes. It does not need to be an exhaustive essay, however a PR with no description or just a few words might not get accepted, simply because very basic information is missing.

A good description helps us to review and understand your proposed changes. For example, you could say a few words about

* what you try to achieve (new feature, fixing a bug, refactoring, security enhancements, etc.)
* how your code works (short technical summary - focus on important aspects that might not be obvious when reading the code)
* testing you performed, known limitations, open ends you possibly could not solve.
* any areas where you like to get help from an experienced maintainer

### Updating your code

While the PR is open - and under review by maintainers - you may be asked to modify your PR source code.
You can simply update your own branch, and push changes in response to reviewer recommendations. 
GitHub will pick up the changes so your PR stays up-to-date.

!!! warning "Do not use force-push while your PR is open!"
    * It has many subtle and unexpected consequences on our GitHub repository.
    * For example, we regularly lost review comments when the PR author force-pushes code changes. So, pretty please, do not force-push.

You can find a collection of very useful tips and tricks here: [How to properly submit a PR](https://github.com/wled-dev/WLED/wiki/How-to-properly-submit-a-PR)

The 🐰 (see AI) will review each commit, please process the review recommendations.

### Merge a PR

Before merging a PR back into main ask the 🐰 the following:

@coderabbitai, I am about to merge this PR. Please produce three outputs:

1. **PR review** — in-depth review of all commits: a concise summary of what changed and why, a merge recommendation, and a prioritised list of follow-up actions. For the most urgent items (blockers or high-risk changes), include a ready-to-paste prompt that a Claude Code agent can execute immediately before merge.

2. **End-user docs prompt** — a ready-to-paste prompt for a Claude Code agent to update `/docs/endusers`. Rules: only describe usage implications (what changed for the user); no internals, no code, no architecture; check existing pages before adding — update in place rather than duplicating; keep additions compact and user-friendly.

3. **Developer docs prompt** — a ready-to-paste prompt for a Claude Code agent to update `/docs/developers`. Rules: target contributors, not end users; be concise — if the detail is already in the code or commit messages, do not repeat it; focus on decisions, patterns, and guidance that are not obvious from reading the source.



# Standards and Guidelines

## Development Principles

| Principle | Detail |
|-----------|--------|
| **Library API stability** | The public `initled()` / `showPixels()` / `setBrightness()` surface must not break existing sketches. New parameters must have defaults. |
| **Docs with every PR** | Functionality and documentation go in the same Pull Request. |
| **Mark non-trivial changes** | Use `// 🌙` for changes required by a specific IDF version. Use `// 💫` for features added by the MoonModules fork. This makes upstream merges traceable. |
| **Main branch is protected** | No direct code commits to `main`. Branch → PR → merge. Doc-only changes may go directly to `main` (docs folder = MkDocs website source). |
| **Compilable PRs** | Every PR must compile cleanly across **all four** PlatformIO environments: `esp32dev`, `esp-wrover-kit`, `esp32-s3-devkitc-1`, `esp32-p4`. Work-in-progress code is acceptable if it compiles without errors. |
| **Platform branching** | All hardware-specific code is guarded by `CONFIG_IDF_TARGET_ESP32`, `CONFIG_IDF_TARGET_ESP32S3`, or `CONFIG_IDF_TARGET_ESP32P4`. No chip-specific logic in shared paths. |
| **No mutable globals** | State lives on the `I2SClocklessLedDriver` class, not file-scope statics. See the `gNbDmaBuffer`/`gNumStrips` removal (commit 54db938) for the rationale. |

### Where code lives

```
src/              Library source
  I2SClocklessLedDriver.h    Main class + ESP32/S3 I2S implementation
  I2SClocklessLedDriver.cpp  updateDriver() / deleteDriver()
  parlio_p4.h/.cpp           ESP32-P4 PARLIO implementation
  pixeltypes.h               Pixel / Pixels types
  framebuffer.h              Double-buffer helper
  HardwareSprite.h/.cpp      Optional sprite overlay
  helper.h                   Timing macros
  main.cpp                   Dev/test sketch (excluded from library build)

docs/
  index.md                   Overview + supported targets
  enduser/enduser.md         End-user API reference
  developer/developer.md     Architecture + repo reorg plan
  developer/standardsandguidelines.md   This file

.github/workflows/
  build.yml                  Compile all four environments on every push/PR
  lint.yml                   cppcheck + clang-tidy on every push/PR
  docs.yml                   Build + deploy MkDocs to GitHub Pages on push to main
```

---

## Pull Requests

### Creating a PR

Always target the `main` branch. Create a feature branch first, then open the PR.

**Every PR description should cover:**

- What you're trying to achieve (new feature, bug fix, refactor, …)
- How it works — a short technical summary of non-obvious aspects
- Which environments were tested (which boards/IDF versions)
- Any areas where you'd like reviewer help

A PR with no description or just a few words may not get accepted.

### Updating an open PR

Push additional commits to your branch — GitHub keeps the PR up-to-date automatically.

!!! warning "Do not force-push while your PR is open"
    Force-pushing causes review comments to disappear and has other subtle side effects on the repository history.

The 🐰 (CodeRabbit) reviews each commit — address its recommendations before requesting a merge.

### Merging a PR

Before merging, ask CodeRabbit:

```
@coderabbitai, I am about to merge this PR. Please produce three outputs:

1. **PR review** — in-depth review of all commits: a concise summary of what changed
   and why, a merge recommendation, and a prioritised list of follow-up actions.
   For the most urgent items (blockers or high-risk changes), include a ready-to-paste
   prompt that a Claude Code agent can execute immediately before merge.

2. **End-user docs prompt** — a ready-to-paste prompt for a Claude Code agent to update
   `/docs`. Rules: only describe usage implications (what changed for the user);
   no internals, no code, no architecture; check existing pages before adding —
   update in place rather than duplicating; keep additions compact and user-friendly.

3. **Developer docs prompt** — a ready-to-paste prompt for a Claude Code agent to update
   `/docs/developer`. Rules: target contributors, not end users; be concise — if the
   detail is already in the code or commit messages, do not repeat it; focus on
   decisions, patterns, and guidance that are not obvious from reading the source.
```

---

## Artificial Intelligence

AI tooling is used in this project. Every contributor remains responsible for the code they submit.

> **Most important principle: the library must never depend on AI.**  
> Every workflow must remain fully functional without any AI tool.

### AI Principles

Five principles govern all AI use: **4EP · Static analysis · Reversible · Attribution · Documentation**

---

**4 Eyes Principle (4EP)**

AI-generated code must always be reviewed by a human before it lands in the repo:

1. Developer reviews Claude Code output 👀 before committing to a feature branch
2. CodeRabbit automatically reviews each commit 👀 in the PR
3. Developer processes and resolves CodeRabbit's findings

---

**Static Analysis**

There are no automated unit tests (validation requires real hardware). Static analysis is the compensating control.

| Tool | What it checks | Where |
|------|---------------|--------|
| **cppcheck** | Logic errors, undefined behaviour, memory leaks | `lint.yml` CI |
| **clang-tidy** | Style, naming, modernisation | `lint.yml` CI (cross-compilation errors suppressed — see developer.md) |
| **PlatformIO build** | All four environments compile cleanly | `build.yml` CI |

Run locally before pushing:
```bash
pio run                        # build all environments
cppcheck --enable=all src/     # static analysis
```

---

**Reversible**

- Development must never depend on any AI tool
- Committing without AI assistance must always be possible
- Any AI tool can be replaced or removed at any time

---

**Attribution**

AI models are trained on third-party code and research. When that knowledge surfaces here, document the source — link to the GitHub repo, algorithm paper, or website the AI drew from. See `parlio_p4.cpp` file header for an example.

---

**Documentation**

AI-generated code must be documented to the same standard as human-written code.

---

### Contributing with AI

Using AI assistance is fine. As the contributor, you are still responsible for the code:

- **Understand it** — do not accept AI output because it "seems to work"
- **Review changes to existing code** — AI edits can silently drop comments or break subtle logic; pay particular attention to ISR-path functions (`loadAndTranspose`, `i2sStop`, `interruptHandler`)
- **Verify platform guards** — AI often forgets `#ifdef CONFIG_IDF_TARGET_*` when adding a new branch; always check that ESP32, S3, and P4 each compile

Mark larger AI-generated sections with a comment:
```cpp
// Below section generated with AI assistance
```

---

### AI Models

> **Status: April 2026.** AI models evolve rapidly — re-evaluate model choices every few months.

#### Claude Code models

Claude Code is a CLI tool (`claude`) that integrates with your terminal and editor.

| Model | ID | Speed | Use when |
|-------|----|-------|----------|
| **Opus 4.6** | `claude-opus-4-6` | Slower | Hardest tasks: architecture, cross-cutting refactors, platform porting, complex debugging |
| **Sonnet 4.6** | `claude-sonnet-4-6` | Fast | Default workhorse — 90% of tasks |
| **Haiku 4.5** | `claude-haiku-4-5-20251001` | Fastest | Quick lookups, single-line fixes, bulk repetitive edits |

Switch model with `/model <model-id>` inside a Claude Code session. Enable Fast mode with `/fast` (same Opus model, faster output).

---

#### Mistral models

Mistral's vibe coding approach works best via [Le Chat](https://chat.mistral.ai) or the Mistral API.

| Model | Use when |
|-------|----------|
| **Mistral Large** | Complex reasoning, architectural questions, cross-file analysis |
| **Codestral** | Code generation and completion — optimised for code |
| **Mistral Small** | Fast iteration, quick Q&A, low-cost tasks |
| **Pixtral** | When you need to attach a screenshot or image (e.g. schematic, UI mockup) |

---

#### Task → model guide

**Add platform support for a new ESP32 variant**

> Complexity: high — touches header, .cpp, parlio/i2s driver, docs  
> Model: **Opus 4.6** / **Mistral Large**

Prompt pattern:
```
Add support for CONFIG_IDF_TARGET_ESP32XX following the pattern used for ESP32-P4.
The peripheral is [describe: I2S / PARLIO / other].
Add #ifdef guards in setPins(), initLedImpl(), showPixelsImpl().
Create src/[driver]_esp32xx.h with hwInit(), initTransferBuffers(),
loadAndTranspose(), hwStart(), hwStop().
Update platformio.ini, docs/developer/developer.md, docs/enduser/enduser.md.
```

---

**Extend an existing initled() overload or add a new one**

> Complexity: low–medium — scoped to public API  
> Model: **Sonnet 4.6** / **Codestral**

Prompt pattern:
```
Add an initled() overload that accepts [describe new parameters].
It must call the canonical initled(leds, pinsq, sizes[], numStrips,
nbComponents, pR, pG, pB, pW, pW2) after translating its arguments.
Do not change initLedImpl() or any platform-specific code.
```

---

**Fix a bug in the transposition or ISR path**

> Complexity: high — ISR-safe code, timing-sensitive  
> Model: **Opus 4.6** / **Mistral Large**

Prompt pattern:
```
In [loadAndTranspose / interruptHandler / i2sStop], the following behaviour
is wrong: [describe symptom, reproduce steps, any decoded stack trace].
Relevant members: [list].
Fix must be ISR-safe (no malloc, no blocking FreeRTOS calls).
Do not change the function signature.
```

---

**Update or add documentation**

> Complexity: low  
> Model: **Sonnet 4.6** / **Mistral Small**

Prompt pattern:
```
Update docs/enduser/enduser.md and/or docs/developer/developer.md to reflect:
[describe what changed].
Rules: end-user doc = usage only, no internals; developer doc = decisions and
patterns not obvious from source. Update in place — do not duplicate existing sections.
```

---

**Debugging a crash / hard fault**

> Complexity: high — needs full context  
> Model: **Opus 4.6** / **Mistral Large**

Prompt pattern:
```
The ESP32 crashes with the following decoded stack trace:
[paste trace]
The crash happens after [describe steps].
Relevant files: [list files and line numbers].
Identify the root cause and suggest a minimal fix.
Constraint: the fix must not affect the ISR hot path performance.
```

---

## Code Style

### Formatting

| Tool | When to run |
|------|-------------|
| Clang-format (see `.clang-format`) | Before every commit — right-click → Format Document in your IDE |

### Comments

Every non-trivial function should have a comment describing what it does and what its arguments mean.

```cpp
// Single-line comment.

/*
 * Multi-line comment — used for file headers and function explanations.
 * Wrap at ~100 characters.
 */
```

Inline comments are fine when they describe only that line and are not too wide.

### Change markers

Use these emoji markers in comments to make diffs and upstream merges traceable:

| Marker | Meaning |
|--------|---------|
| `// 🌙` | Change required for a specific IDF version (state which: `IDF5.5: 🌙 …`) |
| `// 💫` | Feature or fix added by the MoonModules fork (e.g. variable strip lengths, RGBCCT) |

Example:
```cpp
// IDF5.5: 🌙 use gpio_iomux_output instead of gpio_iomux_out
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    gpio_iomux_output((gpio_num_t)pinsq[i], PIN_FUNC_GPIO);
#else
    gpio_iomux_out(pinsq[i], PIN_FUNC_GPIO, false);
#endif

// 💫 Pad short strips to zero so they don't display garbage
memset(&mappedBuffer[pin * COMPONENTS_PER_PIXEL], 0, COMPONENTS_PER_PIXEL);
```

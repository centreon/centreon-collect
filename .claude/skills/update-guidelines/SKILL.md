---
name: update-guidelines
description: Update the architecture description or the development guidelines of centreon-collect (ARCHITECTURE.md, docs/architecture/*.md, docs/conventions/*.md, docs/testing.md), for one component or for the whole repository, and hand over to /create-adr when the change is structuring. Use when the user wants to add, change or remove a convention, a review criterion, a test rule or a piece of architecture description, or says "update the architecture", "update the conventions", "add a rule", "mets à jour l'architecture", "mets à jour les guidelines", "ajoute une convention".
argument-hint: "[change in a few words] [Jira key]"
---

# Update the architecture or the guidelines

The rules on these files are in [`AGENTS.md`](../../../AGENTS.md) (working agreements), [`ARCHITECTURE.md`](../../../ARCHITECTURE.md) (components, zones, review criteria) and [`docs/adr/README.md`](../../../docs/adr/README.md) (when a decision needs an ADR). Read them first and follow them; this skill only adds the steps. Never restate or alter their rules here or in the files you write.

## 1. Gather the change

From the arguments and the conversation, collect:

- the change, as one or a few sentences in the style of the target files;
- why: the problem, the review remark or the incident behind it;
- its kind: **description** (what the code does: flow, contract, component, known limit) or **rule** (convention, review criterion, test rule, agent wiring);
- the Jira key of the pull request.

Ask the user for what is missing, in one question. Do not invent a rule the user did not state: propose a sentence and let them correct it.

Where it lands, per `AGENTS.md`:

- a description made wrong by a code change in progress goes into that code pull request: stay on the current branch and skip step 6;
- a rule, or a description changed on its own, goes into its own pull request.

## 2. Find the scope: one component or the whole repository

Decide from the Components table and the Zones of `ARCHITECTURE.md`:

| The change concerns | File |
|---|---|
| One documented component (CMA, Engine, Broker) | `docs/architecture/<component>.md` or `docs/conventions/<component>.md`, the section it changes |
| Every component, or code shared between them (`common/`, `clib/`) | `docs/conventions/global.md` |
| A contract between components, the dependency direction, the review criteria, the build, the CI | `ARCHITECTURE.md` |
| Tests or CI dispatch | `docs/testing.md`, in the component section when it has one |
| A component marked "to write" | Ask: the global file if the rule holds for every component, otherwise the component's own files (step 4) |

A rule written for one component never goes into the global file "for later", and a global rule is not copied into each component file. Put it in the most specific file that covers its whole scope; elsewhere, link it.

When the scope is not clear from the change (a rule stated for the CMA that would hold for Engine and Broker too, a description that spans two components), ask the user in one question, with your recommendation first. Name the files and sections you will change before writing.

## 3. Check whether it needs an ADR

Apply "When to write one" of `docs/adr/README.md` to the change, and also: does it contradict an ADR linked from the section? That always needs a new ADR that supersedes it.

- No criterion holds (a style rule, a local choice, a description of existing code): say so in one line and go on.
- At least one criterion holds, or you cannot tell: tell the user which criterion and why (options debated, cost of going back, choice that will be questioned), and ask them to confirm the ADR. Never write an ADR without that confirmation, and never skip one the user asks for.

## 4. Write the files

On a new branch from an up-to-date `develop` (`<Jira key>-<slug>`, or the user's branch name), except for the code pull request case of step 1:

1. The sections found in step 2: the smallest change that states the rule or the description. Follow the style of the file: English, short imperative sentences, the tables already there. A rule that a tool checks is not added (`ARCHITECTURE.md` § Checked by tools).
2. A new rule that existing code breaks: add that code to the "Known deviations" table of the conventions file, so that it is not copied.
3. A component that gets its own files: do what `ARCHITECTURE.md` lists for it (Components table, `.claude/hooks/component-context.sh` mapping, folder `CLAUDE.md`, rule in `.claude/rules/`), and run `.claude/hooks/test-component-context.sh`.
4. A file or section renamed or moved: update every link to it (`grep -rn` on the old name in `*.md`, `.claude/`).

Change nothing else. Show the diff to the user and apply their corrections before committing.

## 5. Hand over to `/create-adr`

Only when the user confirmed an ADR in step 3. Invoke the `create-adr` skill on the same branch, with:

- the decision, the context, the alternatives and the Jira key gathered so far;
- the section changed in step 4, as the section to link from;
- the note that the rewrite of that section is already on the branch and belongs to the same pull request, as the user agreed.

`/create-adr` writes the ADR, its line in § Records, and links it from the section. Check that the section states the rule and links `ADR-NNNN` (relative link: `../adr/NNNN-<slug>.md` from `docs/*/`, `docs/adr/NNNN-<slug>.md` from `ARCHITECTURE.md`). `/create-adr` opens the pull request: skip step 6.

## 6. Open the pull request

- Commit and pull request title: `docs(<scope>): <change>`, with the component scope of the conventions (`cma`, `engine`, `broker`...), or `docs` when the change is global.
- A draft pull request targeting `develop`, one change per pull request. Fill in `.github/PULL_REQUEST_TEMPLATE.md`:
  - Description: the change, why, the files changed, and "No ADR: <reason>" when step 3 found none;
  - `Fixes` the Jira key;
  - type "Updating documentation";
  - testing procedure: review by the people the change concerns.
- Tell the user who must approve it: the CODEOWNERS of the changed files (`.github/CODEOWNERS`) and, for a component, the owners named in "Changes that need the owner's agreement first" of its architecture file.

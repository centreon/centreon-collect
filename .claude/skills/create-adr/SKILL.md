---
name: create-adr
description: Draft an Architecture Decision Record for centreon-collect and open it as its own draft pull request. Use when the user wants to record, propose or supersede a structuring decision (protocol, language, exchange format, threading model, dependency, compatibility policy), or says "create an ADR", "write an ADR", "rédige un ADR", "trace cette décision".
argument-hint: "[decision in a few words] [Jira key]"
---

# Create an ADR

The template, the criteria and the lifecycle are in [`docs/adr/README.md`](../../../docs/adr/README.md). Read it first and follow it; this skill only adds the steps. Never restate or alter the template here or in the ADR.

## 1. Gather the decision

From the arguments and the conversation, collect:

- the decision, as one imperative sentence;
- the context: the problem, the current practice, why it is not settled;
- the alternatives weighed and why each lost;
- the consequences and the exceptions;
- the facts that would reopen it;
- the Jira key of the pull request;
- the ADR it supersedes, if any.

Ask the user for what is missing, in one question. Do not invent alternatives or consequences: an ADR records what people decided.

## 2. Check that it is an ADR

Apply "When to write one" of `docs/adr/README.md`. If no criterion holds (a style rule, a local choice), say so and propose instead the sentence to add to the matching `docs/conventions/*.md` or `docs/architecture/*.md` file. Stop there unless the user insists.

## 3. Find where it goes

- Components: read `ARCHITECTURE.md` and the architecture and conventions files of the components the decision touches. Name them to the user.
- Section to link from: the section of those files the decision changes. If none exists, name the file and the place where a one-line rule will go.
- Number: the highest `NNNN` among `docs/adr/[0-9][0-9][0-9][0-9]-*.md` plus one, on four digits, starting at `0001`. Check open pull requests too, so that two drafts do not take the same number:

  ```bash
  gh pr list --state open --search "ADR- in:title" --json number,title
  ```

- Slug: the decision in kebab-case, a few words, ASCII (`send-agent-results-as-otlp`).

## 4. Write the files

On a new branch from an up-to-date `develop` (`adr-<NNNN>-<slug>`, or the user's branch name):

1. `docs/adr/NNNN-<slug>.md`: the template of `docs/adr/README.md`, filled in English. `Status: Proposed`. `Date`: today, or the real date of the decision when it was taken earlier.
2. `docs/adr/README.md` § Records: replace "None yet." or append `- [ADR-NNNN: <title>](NNNN-<slug>.md) — Proposed`.
3. The architecture or conventions section found in step 3: the rule in one line, with a link to the ADR. When the decision changes more than that line, stop and ask: the rewrite belongs in the same pull request only if the user agrees.
4. Superseding: in the old ADR, change only the status line to `Superseded by ADR-NNNN` and the status in § Records. Never rewrite its body.

Change nothing else. Show the ADR to the user and apply their corrections before committing.

## 5. Open the pull request

- Commit and pull request title: `docs(<component>): add ADR-NNNN <decision>`. Use the component scope of the conventions (`cma`, `engine`, `broker`...), or `docs(adr)` when several components are concerned.
- A draft pull request targeting `develop`, one ADR per pull request. Fill in `.github/pull_request_template.md`:
  - Description: the decision, why it needed an ADR, the files linked;
  - `Fixes` the Jira key;
  - type "Updating documentation";
  - testing procedure: review by the people the decision concerns.
- Tell the user who must approve it. The CODEOWNERS of `docs/**` are listed in `.github/CODEOWNERS`; the people the decision concerns are named in the component file ("Changes that need the owner's agreement first").
- Once approved, and before the merge, the status becomes `Accepted` in the ADR and in § Records, in a last commit of the same pull request. Remind the user of this step; do not do it before the approval.

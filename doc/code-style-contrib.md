<!-- SPDX-License-Identifier: CC-BY-4.0 -->


# Contribution Guidelines and Coding Style for Developers

This document lays out the basic contribution policies for developers. It
describes the generel workflow, describes some coding rules, and how code
review is performed.

[[_TOC_]]

## General

OpenAirInterface employs both human review and automated CI tests to judge
whether a code contribution is ready to be merged.

The contributor has to sign a contributor license agreement (CLA) as described
in [`CONTRIBUTING.md`](../CONTRIBUTING.md). After creating an account on the
Eurecom Gitlab, the contributor can open a merge request: he becomes the
"author" of such code contribution. A senior OAI member will review this work,
and make suggestions for possible improvements. Each week, we discuss the
progress of the merge requests in a [weekly external developer
call](https://gitlab.eurecom.fr/oai/openairinterface5g/-/wikis/OpenAirDevMeetings),
and discuss which merge requests can be merged.

The CI consists in various Jenkins pipelines that run on each merge request.
See [`TESTBenches.md`](./TESTBenches.md) for more details about the CI setup.

There is the official [Gitlab Help](https://docs.gitlab.com/) that can help you
with any questions regarding Gitlab. We recommend reading the [Git
Book](https://git-scm.com/book/en/v2) to use Git properly.

## Basic coding rules

You should respect the `.clang-format` file in the root of the repository. The
`clang-format` tool will pick up this file when being applied to code in the
repository. Please also refer to the [corresponding
documentation](./clang-format.md).

A number of high-level comments:

- Indentation is two spaces, no tabs; try to limit the number of indentations.
- Line length is 132, not more than one statement per line; no whitespace at
  the end of lines
- The opening brace after a function is on a new line; after control flow
  statements (`if`, `while`, `switch`, ...), it is on the same line
- Pointer or reference operators (`*`, `&`) are right-aligned
- Do not commit code that is commented out
- Use strong typing (no `void *`, use complex data types such as `c16_t` over
  `uint32_t` in L1, ...)
- Do not use [magic numbers](https://en.wikipedia.org/wiki/Magic_number_(programming)#Unnamed_numerical_constants)
  for unnamed numerical constants and do not hardcode values
- Don't cast the result of `malloc()`: it is not needed, and can lead to bugs.
- Use `AssertFatal()` and `DevAssert()` to check for invariants, not for error
  handling: Assertions are for preventing bugs (e.g., unforeseen state),
  not to sanitize input.
- Use `const` on pointer function arguments that are input to that function;
  put output variables (via a pointer) last
- Do not do premature optimization; measure the code before writing SIMD
  instructions by hand, and measure again to show it is faster.

If in doubt, check out code that has been recently written (e.g., use the merge
requests page to check for code that has recently been added) and follow that
style. Checking surrounding code is usually not the best idea, as OAI has a
long history in which coding rules were not really enforced.

There is an old [OAI coding guidelines
document](https://gitlab.eurecom.fr/oai/openairinterface5g/-/wikis/documents/openair_coding_guidelines_v0.3.pdf)
that might be useful; if this document and `.clang-format` contradict,
`.clang-format` takes precedence.

## Main Workflow and Versioning

### Workflow

You should be familiar with git branching, merging, and rebasing.

The target branch for every contribution, and the general development branch,
is `develop`. Typically every week, we collect multiple merge requests in an
"integration branch" that gets tested by the CI individually. If everything is
fine, we merge to develop and tag it `YYYY.wXX` with `YYYY` the current year,
and `XX` the week number.

Note that the above includes a lot of merging, making the Git history difficult
to understand. To not needlessly increase the complexity, please keep your
branch history linear (i.e., no merges).  Each commit should be able to
compile, and ideally be able to run an End-to-End test of gNB/UE using RFsim.
This can be achieved by making each commit a **logical change** to be applied to
the code base, which also facilitates the review of your changes. The Linux
kernel has some [documentation on what a logical change
is](https://www.kernel.org/doc/html/latest/process/submitting-patches.html#separate-your-changes).
From a practical point of view, this means that your history should not have
commits that "clean up" a previous commit (indicated by commit messages such as
`Fix bug` or `Review addressed`). They don't describe what the fix is about,
and make review more difficult because the changes are not self-contained, but
spread across commits, incurring mental overhead. Instead, the commit series
should be written from the first commit as if you knew how the final code looks
like. In other words, you should guide the reader of your commits. This includes
that every commit message describes _why_ a particular change is necessary and
correct(!). Note that the rule of making commits small still applies! In
summary:

- Make logical changes, which are **small, self-contained** commits,
- Don't fix up changes later: **rewrite the history** to guide the
  reader/reviewer, and
- In the commit message, **explain why** changes are necessary and correct.

A commit message can (and often should) take several lines.  One-line commit
messages should be reserved for very simple changes. If in doubt, prefer to
explain your work more than less.

The workflow of the integration branch has its weaknesses; we might revise this
in the future towards a workflow using rebase to integrate work of different
people, in order to simplify the history, and allow the usage of tools such as
`git bisect` to search for bugs.

After some time, we make a stable release. For this, we simply merge develop
into master, and give a semantic versioning number, e.g., `v1.1`. We target to
make releases bi-yearly.

### How to manage your own branch

Before starting to work, please make sure to branch off the latest `develop`
branch.  Make commits as appropriate.
```bash
$ git fetch origin
$ git checkout develop
$ git checkout -b my-new-feature # name as appropriate
$ git add -p                     # add changes for change set 1, use `-p` to review what to include
$ git commit                     # in the editor, describe your changes
$ git add -p                     # add changes for change set 2
$ git commit                     # in the editor, describe your changes
```

Again, commit message should take multiple lines; after the initial title, a
blank line should follow. Read the `DISCUSSION` section in `man git commit` for
more information.

If your development takes longer, make sure to synchronize regularly with
`origin/develop` using `git rebase`:
```bash
$ git fetch origin
$ git rebase -i origin/develop
```

If you do logical changes, you should not have to resolve the same conflicts
over and over again. Note that if you jumped over multiple develop tags, you
can also rebase in intermediate steps, in case you fear the differences might
be too big.
```
$ git rebase -i 2023.w38
$ git rebase -i 2023.w41
$ git rebase -i develop
```

Once you rebased, push the changes to the remote
```
$ git push origin my-new-feature --force-with-lease # force with lease let's you only overwrite what you also have locally in origin/my-new-feature
```

## Merge Requests

A merge request (MR) can be submitted as soon as the code is considered stable
and reviewable. The idea is to start the review early enough so that the code
author (the MR owner) can incorporate fixes while the reviewer is giving
feedback. Note that while it should not be common, a refusal of a merge request
is a valid outcome of a merge request review (subject to proper justification).

When preparing a contribution that is large, the developer is responsible for
warning the OAI team, so that the review work can start as early as possible
and run in parallel to the contribution finalization. Failing to do so, there
is a risk that the work will take a long time to be merged or might even not be
merged at all if judged too complex by the OAI team. Also, note that big
contributions should be cut into small commits each containing a logical
change, as described above. Finally, as a rule of thumb, the smaller the merge
request, the easier it will be to review and merge.

The reviewer comments on code changes ("open comments") that should be
addressed by the author. Most reviewers prefer to mark open comments as
resolved by themselves to double check the modifications and close such
comments. As an author, please don't resolve open comments (don't click the
"Resolve thread" button) unless explicitly instructed by the reviewer.

Note that the _merge request author_ asks for inclusion of code, so _they
should make the review easy_; in particular, if facilitating review incurs
extra work to make a simpler code review (e.g., rewriting entire commits or
their order), this extra work is justified. This particularly(!) applies for
big merge requests.

When opening a merge requests, the author should select `develop` as the target
branch, and add at least one of these labels when opening the merge request:

- ~documentation: don't perform any testing stages, for documentation
- ~BUILD-ONLY: execute only build stages, for code improvements without impact
  on 4G or 5G code
- ~4G-LTE: perform 4G tests
- ~5G-NR: perform 5G tests

Failure to add a label will prevent the CI from running. You can add both
~4G-LTE and ~5G-NR together; if in doubt about the right label, add both. The
CI posts the results in the comments section of the merge request. Both merge
request authors and reviewers are responsible for manual inspection and
pre-filtering of the CI results. An overview of the CI tests is in
[`TESTBenches.md`](./TESTBenches.md).

To communicate the review progress both between author and reviewer, as well as
to the outside world, we (ab-)use the milestones feature of Gitlab to track the
current progress. The milestone can be set when opening the merge request, and
during its lifetime in the sidebar on the right. Following options:

- _no milestone_: not ready for review yet and is generally used to wait for a
  first CI run that the author will inspect and fix problems detected by the CI
  (please limit the time in which your code is in that phase)
- %REVIEW_CAN_START: the reviewer can start the review
- %REVIEW_IN_PROGRESS: the reviewer is currently doing review, and might
  request changes to the code that the author should include (or refute with
  justification)
- %REVIEW_COMPLETED_AND_APPROVED: the reviewer is happy with code changes
  (*open comments still have to be addressed!*)
- %OK_TO_BE_MERGED: the OAI team plans to merge this; *do not push any changes
  anymore at this point*.

## Review Form

The following is a check list that might be used by a reviewer to check that
code contribution fulfils minimum standard w.r.t. formatting, data types,
assertions, etc. The reviewer might copy/paste this form into a merge request,
or simply check that all have been filled.

All points should be marked to complete a review.

```md
### Review by @username

- [ ] `.clang-format` respected
- [ ] No merges, i.e., the branch has a linear history; every commit compiles (and ideally runs in RFsim)
- [ ] For L1: uses complex data types. In general: prefers strong/adapted types/typedefs over `void`/generic `int`, or otherwise primitive types.
- [ ] Documentation updated (doxygen summary of functions; in `doc/`, or the corresponding folder; `FEATURE_SET.md`)
- [ ] Uses assertions were appropriate
- [ ] No commented/dead code (or such code has been removed), no function duplication
- [ ] No magic numbers: use defines, enums, and variables to make the meaning of a number clear.
- [ ] The changes don't have patch noise (no unnecessary whitespace changes unrelevant to the changed code; reformatting is ok)
- [ ] No unnecessary/excessive logs introduced. Prefer LOG_D for frequent logs

Additional remarks, if applicable:
```

Additional optional questions in case they apply:
- Has a new tool/dependency been introduced? Needs to be discussed if to be added.
- Is a new CI test necessary? Can it be done in simulators?

## Reporting bugs

Please report only true bugs in the [issue tracker](../../issues). Do not
report general user problems; use the [mailing
lists](https://gitlab.eurecom.fr/oai/openairinterface5g/-/wikis/MailingList)
instead.  If in doubt, prefer the mailing lists and if needed and requested by
the OAI team, an issue will be opened.

When reporting a bug, please clearly
- explain the problem,
- note what you expected to happen (what should happen),
- show what happens instead (what did happen), and
- give steps on how to reproduce (including, if needed, configuration files and
  command lines).

You are encouraged to use these bullet points to structure your issue for easy
understanding.  Use code tags (the "insert code" button with symbol &lt;/&gt; in
the gitlab editor) for logs and small code snippets.

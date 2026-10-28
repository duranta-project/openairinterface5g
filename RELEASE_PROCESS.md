<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Release Process

This document describes the release process for the Duranta
`openairinterface5g` project.

## Responsibilities

Project maintainers coordinate the release process, review release
preparation changes, and ensure that release artifacts are published
and announced.

Community members can contribute by testing release candidates and
Docker images, and reporting issues.

## Prepare the Release

- [ ] **Create a GitHub milestone:** Create a
      [milestone](https://github.com/duranta-project/openairinterface5g/milestones)
      for the upcoming release, following [Semantic Versioning](https://semver.org/) (`MAJOR.MINOR.PATCH`).
- [ ] **Update the feature set:** Update [doc/FEATURE_SET.md](doc/FEATURE_SET.md).
- [ ] **Update the changelog:** Update [CHANGELOG.md](./CHANGELOG.md) with
      the release version, date, and notable changes.
- [ ] **Update the software version:** Set `OAI_VERSION` in
      [CMakeLists.txt](CMakeLists.txt).
- [ ] **Update the README:** Update the release version in [README.md](./README.md).
- [ ] **Verify version consistency:** Ensure that all version references
      are consistent.
- [ ] **Prepare the GitHub release:** Create a draft release with the
      target version and proposed release notes.
- [ ] **Submit the release PR:** Open a PR containing the release
      preparation changes for review.
- [ ] **Verify CI status:** Confirm that the required CI checks pass.

## Release Notes

Include the following information in the GitHub release description:

- **Summary:** Briefly describe the release and its main changes.
- **Features and improvements:** List notable new features and improvements.
- **Bug fixes:** Summarize important bug fixes.
- **Breaking changes:** Highlight changes that may affect existing users.
- **Known issues:** Document any known issues or limitations.
- **Docker images:** Provide links to the published Docker images and tags.
- **References:** Link to the changelog and feature set.

Use `CHANGELOG.md` as the primary reference for the release notes.

## Create and Verify the Release

After the release PR has been merged:

- [ ] **Create the release tag:** Tag the commit containing the merged
      release PR, following the project's release tag convention.
- [ ] **Verify the release tag:** Confirm that the tag points to the
      intended commit and matches the release version.
- [ ] **Publish Docker images:** Use the
      [Jenkins release pipeline](https://jenkins-oai.eurecom.fr/job/RAN_Publish_Release_Images/)
      to publish the release images to
      [Docker Hub](https://hub.docker.com/u/oaisoftwarealliance).
- [ ] **Verify Docker images:** Confirm that all expected images and tags
      are available, can be pulled, and support the expected architectures.
- [ ] **Update Docker Hub information:** Update repository descriptions
      and release information as needed.

## Announce the Release

- [ ] **Publish the GitHub release:** Publish the draft release with the
      correct tag, title, and release notes.
- [ ] **Announce the release:** Create a
      [GitHub Discussion](https://github.com/duranta-project/openairinterface5g/discussions)
      announcing the release and linking to the release notes and Docker images.
- [ ] **Close the milestone:** Close the completed GitHub milestone.

## Reporting Issues

Report release-related issues through the project's
[GitHub issue tracker](https://github.com/duranta-project/openairinterface5g/issues).

Include the release version, affected component, environment, steps to
reproduce, expected and actual behavior, and relevant logs. Check for
existing reports before opening a new issue.

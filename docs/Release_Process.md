# Release Process

How to cut an FPP release build (beta or stable) using the GitHub Actions
pipeline in `.github/workflows/build-images.yml` and `docker-build.yml`.

## Branch and tag conventions

- **One branch per minor series**, not per release build. e.g. `v10.1` covers
  every `10.1.x` release; betas for a not-yet-stable series share a single
  branch for their whole beta run (the `10.0` series uses `v10.0-beta` for
  this reason — it predates the `v<major>.<minor>` convention taking effect).
- Users on a release branch continue to receive updates via "Check for
  Updates" (`git pull`/`rebase @{u}`) as more commits land on that branch.
  Tagging a release does **not** freeze the branch — it only marks the
  commit that got built and published. This is intentional: it lets beta
  testers keep getting fixes without re-flashing.
- **Tags are numeric with NO `v` prefix** (`10.0-beta2`, `10.1.2`, not
  `v10.1.2`). `src/fppversion.sh` runs the tag through `git describe`
  verbatim; a leading `v` would corrupt every displayed version string.
- The tag-push trigger in `build-images.yml` only fires for tags whose major
  version is two-or-more digits (`[1-9][0-9]*`, excluding `*-master`
  branch-point markers). Series `9.x` and earlier are not wired for
  automated image builds.

## Before tagging: make sure the branch has everything it needs

The release branch (e.g. `v10.0-beta`) can drift behind `master` if fixes
land on `master` without also being merged/cherry-picked into the release
branch. Before cutting a new tag, check:

```bash
git fetch origin
git log --oneline origin/<release-branch>..origin/master
```

If that shows commits you want in the release, merge `master` into the
release branch **first**, resolve any conflicts, and push that merge before
tagging. Don't tag on top of a stale branch and assume it's current.

## Cutting the release

From the release branch, with everything you want included already merged
and pushed:

```bash
git fetch origin
git checkout -B <release-branch> origin/<release-branch>
git commit --allow-empty -m "<tag>"
git tag -a <tag> -m "<tag>"          # add -s to GPG-sign, if a signing key is configured
git push origin <release-branch> <tag>
```

Pushing the branch and the tag are two separate ref pushes — combine them
in one `git push` invocation (as above) or run them separately, but both
need to happen. The branch push ensures the commit actually exists on the
tracked branch (required for the release build's branch-resolution logic
and for users' self-update to keep working); the tag push is what actually
triggers the release build.

## What happens automatically

Pushing a matching tag triggers `build-images.yml`:

1. **`prep`** — determines this is a release build from the tag name.
2. **`build`** — builds Pi, Pi64, BB64, BBB images in parallel (GitHub-hosted
   `ubuntu-24.04-arm` runners; Pi/BBB run under qemu and take a couple of
   hours each).
3. **`release`** — downloads all build artifacts and creates a **draft**
   GitHub release named `FPP <tag>`, with all four image assets attached.

`docker-build.yml` also triggers off the same tag push and builds container
images independently.

Note: the nightly build (`schedule: cron '0 4 * * *'`) runs against
`master` and uses the same runner type, but has its own `concurrency` group
keyed on `github.ref`, so it never cancels or interferes with a tag-based
release build (or vice versa) — at worst, running at the same time means
both compete for the shared runner pool queue.

## Publishing

Once the `Build Images` workflow run completes successfully:

1. Go to the repo's **Releases** page.
2. Find the **draft** release named `FPP <tag>`.
3. Fill in the release notes.
4. Hit **Publish**.

## Worked example: 10.0-beta → 10.0-beta2

Concrete commands used to cut `10.0-beta2` from the `v10.0-beta` branch,
including merging in commits that had landed on `master` since the branches
diverged:

```bash
# Make sure local state matches origin before starting
git fetch origin
git checkout -B v10.0-beta origin/v10.0-beta

# Bring in fixes that landed on master since the branches diverged
git merge origin/master --no-edit
# -> resolve any conflicts, e.g.:
#      git checkout --theirs <file>   # take master's side
#      git add <file>
#    then:
git commit --no-edit

# Pick up any submodule pointer changes from the merge
git submodule update --init --recursive

# Push the merged branch
git push origin v10.0-beta

# Create the empty release-marker commit and tag it
git commit --allow-empty -m "10.0-beta2"
git tag -a 10.0-beta2 -m "10.0-beta2"

# Push branch + tag together, triggering the release build
git push origin v10.0-beta 10.0-beta2
```

## Notes on GPG signing

`git tag -a -s` requires a GPG secret key configured for the signing user
(`git config user.signingkey`, key present in `gpg --list-secret-keys`). If
no key is set up, `-s` won't produce a valid signature — use a plain
annotated tag (`-a` without `-s`) instead, or set up a key first.

## Announcing the release

Once the release is published on GitHub, announce it in the usual community
channels:

- **Falcon Christmas Forums** — post in the Falcon Player (FPP) board:
  <https://falconchristmas.com/forum/index.php?board=8.0>
  (either start a new topic for the release or reply in the existing
  release-announcements topic, depending on current forum convention).
- **FPP Facebook group**: <https://www.facebook.com/groups/FalconPlayer/>

Include in the announcement:

- The version/tag (e.g. `10.0-beta2`) and a link to the GitHub release page.
- A short summary of what's new/fixed since the previous release — pull
  highlights from the release notes, don't just link and leave it blank.
- For beta releases: a reminder that it's a beta, and how to report issues
  (GitHub issues vs. forum thread, whichever this project prefers).
- Any upgrade caveats (e.g. "existing beta1 installs will pick this up via
  Check for Updates" if applicable, per the branch-tracking behavior
  described above).

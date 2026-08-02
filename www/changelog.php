<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once 'config.php';
    require_once 'common.php';
    include 'common/menuHead.inc';

    $limit = 200;
if (isset($_GET['limit'])) {
    $limit = intval($_GET['limit']);
    // Clamp to a sane range: a zero/negative limit would build "head -0"/"head --5"
    // and silently yield an empty log instead of history.
    if ($limit < 1 || $limit > 2000)
        $limit = 200;
}

$gitDir = dirname(dirname(__FILE__)) . "/.git/";

// Unit-separator (0x1f) delimited fields so PHP can split reliably:
//   hash \x1f author \x1f author-date \x1f subject
$logFormat = '%h%x1f%an%x1f%ai%x1f%s';
$logCmd = "git --git-dir=" . $gitDir . " log --pretty=format:'" . $logFormat . "' | head -" . $limit;

// Full SHA of the checked-out commit, so we can flag the "Current" row.
$currentVersion = "";
exec("git --git-dir=" . $gitDir . " rev-parse HEAD 2>/dev/null", $headOut, $rv);
if ($rv == 0 && isset($headOut[0])) {
    $currentVersion = trim($headOut[0]);
}
unset($headOut);

// Current branch name ("HEAD" when detached, e.g. after a revert).
$currentBranch = "";
exec("git --git-dir=" . $gitDir . " rev-parse --abbrev-ref HEAD 2>/dev/null", $brOut, $rv);
if ($rv == 0 && isset($brOut[0])) {
    $currentBranch = trim($brOut[0]);
}
unset($brOut);

$clickable = ($uiLevel >= 1);

// Fetch the log. On a real branch we cache it to disk so a detached HEAD
// (post-revert) can still show the full branch history instead of only the
// commits up to the reverted point. The cache filename is versioned so a stale
// cache written by an older FPP (different log format) is never misparsed.
$cacheFile = $mediaDirectory . "/tmp/changelog-cache";
$lines = array();
if ($clickable) {
    if ($currentBranch != 'HEAD') {
        exec($logCmd . " | tee " . $cacheFile, $lines);
    } else {
        exec("cat " . $cacheFile . " 2>/dev/null", $lines);
        // No (new-format) cache yet -- e.g. reverted right after an upgrade before
        // ever loading this page on a branch. Fall back to a live log so we show
        // history up to the current point rather than an empty page.
        if (count($lines) == 0) {
            exec($logCmd, $lines);
        }
    }
} else {
    exec($logCmd, $lines);
}

// Build the commit rows.
$commitRows = "";
$commitCount = 0;
foreach ($lines as $line) {
    if (trim($line) === "")
        continue;
    $parts = explode("\x1f", $line);
    if (count($parts) < 4)
        continue;

    list($hash, $author, $adate, $subject) = $parts;
    $hashEsc = htmlspecialchars($hash);
    $shortHash = htmlspecialchars(substr($hash, 0, 7));
    $authorEsc = htmlspecialchars($author);
    $subjectEsc = htmlspecialchars($subject);
    $dateShort = htmlspecialchars(substr(trim($adate), 0, 10));
    $isCurrent = ($currentVersion !== "" && str_starts_with($currentVersion, $hash));
    $commitCount++;

    $rowClass = "fpp-commit";
    if ($clickable)
        $rowClass .= " fpp-commit--clickable";
    if ($isCurrent)
        $rowClass .= " fpp-commit--current";

    // Row carries its data so the modal can show the message without escaping
    // it into inline handlers. htmlspecialchars escapes quotes for attributes.
    $dataAttrs = ' data-sha="' . $hashEsc . '" data-msg="' . $subjectEsc . '"';
    $onclick = $clickable ? ' onclick="openVersionModalFromRow(this);"' : "";

    $currentBadge = $isCurrent
        ? ' <span class="badge rounded-pill text-bg-success ms-2"><i class="fas fa-circle-dot"></i> Current</span>'
        : '';

    $actions = "";
    if ($clickable) {
        $actions = '<div class="fpp-commit__actions" onclick="event.stopPropagation();">'
            . '<a class="fpp-commit__action" href="https://github.com/FalconChristmas/fpp/commit/' . $hashEsc . '" target="_blank" title="View change on GitHub"><i class="fas fa-up-right-from-square"></i></a>'
            . '<span class="fpp-commit__action" title="Revert to this version" onclick="event.stopPropagation(); openVersionModalFromRow(this.closest(\'.fpp-commit\'));"><i class="fas fa-clock-rotate-left"></i></span>'
            . '</div>';
    }

    $commitRows .= '<div class="' . $rowClass . '"' . $dataAttrs . $onclick . '>'
        . '<i class="fas fa-user-circle fpp-commit__avatar"></i>'
        . '<div class="fpp-commit__body">'
        . '<div class="fpp-commit__msg"><span class="fpp-commit__subject">' . $subjectEsc . '</span>' . $currentBadge . '</div>'
        . '<div class="fpp-commit__meta"><span class="fpp-sha-chip">' . $shortHash . '</span>'
        . ' &nbsp;&middot;&nbsp; ' . $authorEsc
        . ' &nbsp;&middot;&nbsp; committed ' . $dateShort . '</div>'
        . '</div>'
        . $actions
        . '</div>';
}
unset($lines);
    ?>
    <title>FPP - ChangeLog</title>
    <script>
        // Selected commit for the version modal's Revert action.
        var clSelectedVersion = null;

        function openVersionModalFromRow(rowEl) {
            if (!rowEl) return;
            openVersionModal(rowEl.getAttribute('data-sha'), rowEl.getAttribute('data-msg'));
        }

        function openVersionModal(version, message) {
            clSelectedVersion = version;
            $('#versionModalSha').text(version);
            $('#versionModalMsg').text(message || '');
            $('#versionModalGithub').attr('href', 'https://github.com/FalconChristmas/fpp/commit/' + version);
            bootstrap.Modal.getOrCreateInstance(document.getElementById('versionModal')).show();
        }

        function RevertToSelectedVersion() {
            bootstrap.Modal.getInstance(document.getElementById('versionModal')).hide();
            GitCheckoutVersion(clSelectedVersion);
        }

        function GitCheckoutVersion(version) {
            // Modern streaming dialog (matches about.php); its Close button
            // handles CloseModalDialog + page reload for us.
            DisplayProgressDialog('gitCheckout', 'Switching to version: ' + version);
            StreamURL('gitCheckoutVersion.php?wrapped=1&version=' + version, 'gitCheckoutText', 'ProgressDialogDone', 'ProgressDialogDone');
        }

        // Live filter over the rendered commit rows (message, author, or SHA).
        function filterCommits() {
            var q = $('#commitFilter').val().toLowerCase();
            var shown = 0;
            $('#commitList .fpp-commit').each(function () {
                var hit = $(this).text().toLowerCase().indexOf(q) !== -1;
                $(this).toggle(hit);
                if (hit) shown++;
            });
            $('#shownCount').text(shown);
        }
    </script>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'help';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">ChangeLog</h1>
            <div class="pageContent">

                <?php if ($clickable) { ?>
                    <!-- Advanced users can revert; explain what clicking a commit does. -->
                    <div class="fpp-banner fpp-banner--info mb-3">
                        <div class="fpp-banner__icon"><i class="fas fa-hand-pointer"></i></div>
                        <div class="fpp-banner__content">
                            <div class="fpp-banner__title">Jump to any version</div>
                            <p class="fpp-banner__message">
                                Click a commit to view its exact changes on GitHub or roll your system
                                back to that point &mdash; your configuration and media are preserved.
                                The highlighted row is the version you're running now.
                            </p>
                        </div>
                    </div>
                <?php } ?>

                <div class="card fpp-card p-0 overflow-hidden">

                    <!-- Toolbar: live filter + current branch -->
                    <div class="d-flex flex-wrap align-items-center gap-3 p-3 border-bottom">
                        <div class="input-group flex-grow-1" style="min-width: 220px;">
                            <span class="input-group-text"><i class="fas fa-search"></i></span>
                            <input type="text" id="commitFilter" class="form-control"
                                placeholder="Filter by message, author, or SHA&hellip;" oninput="filterCommits();">
                        </div>
                        <?php if ($currentBranch != "") { ?>
                            <span class="fpp-text-muted d-inline-flex align-items-center gap-1 text-nowrap">
                                <i class="fas fa-code-branch"></i> Branch: <strong><?= htmlspecialchars($currentBranch) ?></strong>
                            </span>
                        <?php } ?>
                        <?php if ($clickable) { ?>
                            <!-- Pull in changes and switch to the latest commit on this branch.
                                 Also lets a reverted (detached) system return to the branch tip. -->
                            <button type="button" class="fpp-btn fpp-btn--outline text-nowrap"
                                title="Pull in changes and switch to the latest version on this branch"
                                onclick="GitCheckoutVersion('HEAD');">
                                <i class="fas fa-cloud-arrow-down"></i> Update to Latest
                            </button>
                        <?php } ?>
                    </div>

                    <!-- Commit list -->
                    <div id="commitList">
                        <?php
                        if ($commitCount > 0) {
                            echo $commitRows;
                        } else {
                            echo '<div class="p-4 text-center fpp-text-muted">No commit history available.</div>';
                        }
                        ?>
                    </div>

                    <div class="p-3 border-top">
                        <span class="fpp-text-muted">Showing <strong id="shownCount"><?= $commitCount ?></strong>
                            commit<?= $commitCount == 1 ? '' : 's' ?><?php if ($commitCount >= $limit) echo ' (limited to ' . $limit . ')'; ?></span>
                    </div>
                </div>

            </div>
        </div>
    </div>
    <?php include 'common/footer.inc'; ?>

    <!-- Version options modal (shown when a commit row is clicked) -->
    <div class="modal fade" id="versionModal" tabindex="-1" aria-hidden="true">
        <!-- modal-m: FPP sizes modals via .modal-X .modal-content max-width
             (fpp.css forces .modal-dialog nearly full width on desktop). -->
        <div class="modal-dialog modal-dialog-centered modal-m">
            <div class="modal-content">
                <div class="modal-header">
                    <h5 class="modal-title d-flex align-items-center gap-2">
                        <i class="fas fa-code-commit"></i> Commit
                        <span class="fpp-sha-chip" id="versionModalSha"></span>
                    </h5>
                    <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                </div>
                <div class="modal-body">
                    <p class="mb-3" id="versionModalMsg"></p>
                    <p class="fpp-text-muted mb-0">What would you like to do with this version?</p>
                </div>
                <div class="modal-footer">
                    <a class="fpp-btn fpp-btn--outline" id="versionModalGithub" href="#" target="_blank">
                        <i class="fas fa-up-right-from-square"></i> View change on GitHub
                    </a>
                    <button type="button" class="fpp-btn fpp-btn--warning" onclick="RevertToSelectedVersion();">
                        <i class="fas fa-clock-rotate-left"></i> Revert to this version
                    </button>
                </div>
            </div>
        </div>
    </div>
</body>

</html>

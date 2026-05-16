<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - Sound Card Aliases</title>

    <?php $modalMode = isset($_GET['modal']) && $_GET['modal'] == '1'; ?>

    <style>
        .alias-table {
            width: 100%;
            border-collapse: collapse;
            margin-bottom: 1rem;
        }

        .alias-table th {
            background: var(--bs-tertiary-bg, #f8f9fa);
            padding: 0.5rem 0.75rem;
            text-align: left;
            border-bottom: 2px solid var(--bs-border-color, #dee2e6);
            font-weight: 600;
            font-size: 0.9rem;
        }

        .alias-table td {
            padding: 0.5rem 0.75rem;
            border-bottom: 1px solid var(--bs-border-color, #dee2e6);
            vertical-align: middle;
        }

        .alias-table tr:last-child td {
            border-bottom: none;
        }

        .alias-table .card-id-cell {
            font-family: var(--bs-font-monospace, monospace);
            font-size: 0.9rem;
            color: var(--bs-secondary-color, #6c757d);
        }

        .alias-table .card-name-cell {
            color: var(--bs-body-color, #212529);
        }

        .alias-table .alias-input {
            width: 100%;
            min-width: 200px;
            padding: 0.25rem 0.5rem;
            border: 1px solid var(--bs-border-color, #ced4da);
            border-radius: 3px;
            background: var(--bs-body-bg, #fff);
            color: var(--bs-body-color, #212529);
        }

        .alias-table .stale-row {
            opacity: 0.65;
            font-style: italic;
        }

        .alias-actions {
            display: flex;
            gap: 0.5rem;
            justify-content: flex-end;
            margin-top: 1rem;
            flex-wrap: wrap;
        }

        .alias-info {
            background: var(--bs-tertiary-bg, #f8f9fa);
            border-left: 3px solid var(--bs-primary, #0d6efd);
            padding: 0.75rem 1rem;
            margin-bottom: 1rem;
            border-radius: 0 4px 4px 0;
            font-size: 0.92rem;
        }

        .alias-status {
            display: inline-block;
            margin-left: 1rem;
            font-size: 0.9rem;
        }

        .alias-status.success {
            color: #28a745;
        }

        .alias-status.error {
            color: #dc3545;
        }

        <?php if ($modalMode) { ?>
            .modal {
                z-index: 99999 !important;
            }

            .modal-backdrop {
                z-index: 99998 !important;
            }

        <?php } ?>
    </style>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        if (!$modalMode) {
            $activeParentMenuItem = 'status';
            include 'menu.inc';
            ?>
            <div class="mainContainer">
                <div class="title">Sound Card Aliases</div>
                <div class="pageContent">
                <?php } else { ?>
                    <div style="padding: 1rem;">
                    <?php } ?>

                    <div class="alias-info">
                        Assign friendly names to your sound cards so you can identify them in audio device dropdowns.
                        Aliases are keyed on the stable ALSA card ID and apply to both Hardware Direct (ALSA) and
                        PipeWire
                        media backends. Leave an alias blank to remove it.
                    </div>

                    <div id="aliasTableContainer">
                        <div style="padding: 1rem; color: var(--bs-secondary-color, #6c757d);">Loading sound cards…
                        </div>
                    </div>

                    <div class="alias-actions">
                        <span id="aliasStatus" class="alias-status"></span>
                        <button class="buttons btn-outline-secondary" onclick="ReloadAliases()">
                            <i class="fas fa-sync"></i> Reload
                        </button>
                        <button class="buttons btn-outline-success" onclick="SaveAliases()">
                            <i class="fas fa-save"></i> Save
                        </button>
                    </div>

                    <?php if (!$modalMode) { ?>
                    </div><!-- /pageContent -->
                </div><!-- /mainContainer -->
            <?php } else { ?>
            </div><!-- /modal padding -->
        <?php } ?>
    </div><!-- /bodyWrapper -->

    <script>
        var audioCards = [];

        function EscapeHtml(s) {
            if (s === null || s === undefined) return '';
            return String(s)
                .replace(/&/g, '&amp;')
                .replace(/</g, '&lt;')
                .replace(/>/g, '&gt;')
                .replace(/"/g, '&quot;')
                .replace(/'/g, '&#039;');
        }

        function EscapeAttr(s) {
            return EscapeHtml(s);
        }

        function ShowStatus(msg, isError) {
            var el = $('#aliasStatus');
            el.removeClass('success error');
            if (msg) {
                el.addClass(isError ? 'error' : 'success').text(msg);
                if (!isError) {
                    setTimeout(function () {
                        if (el.text() === msg) el.text('').removeClass('success error');
                    }, 3000);
                }
            } else {
                el.text('');
            }
        }

        function RenderAliases() {
            var html = '';
            if (!audioCards.length) {
                html = '<div style="padding:1rem;color:var(--bs-secondary-color,#6c757d);">No sound cards detected.</div>';
                $('#aliasTableContainer').html(html);
                return;
            }
            html += '<table class="alias-table">';
            html += '<thead><tr>';
            html += '<th style="width:8%;">Card #</th>';
            html += '<th style="width:25%;">ALSA ID</th>';
            html += '<th style="width:35%;">Detected Name</th>';
            html += '<th style="width:32%;">Alias</th>';
            html += '</tr></thead><tbody>';
            for (var i = 0; i < audioCards.length; i++) {
                var c = audioCards[i];
                var stale = (c.cardNum < 0);
                html += '<tr' + (stale ? ' class="stale-row"' : '') + '>';
                html += '<td>' + (stale ? '—' : EscapeHtml(String(c.cardNum))) + '</td>';
                html += '<td class="card-id-cell">' + EscapeHtml(c.cardId) + '</td>';
                html += '<td class="card-name-cell">' + EscapeHtml(c.cardName) + '</td>';
                html += '<td><input type="text" class="alias-input" data-card-id="' + EscapeAttr(c.cardId) + '"';
                html += ' value="' + EscapeAttr(c.alias || '') + '"';
                html += ' placeholder="e.g. Transmitter, Amplifier" maxlength="128"></td>';
                html += '</tr>';
            }
            html += '</tbody></table>';
            $('#aliasTableContainer').html(html);
        }

        function ReloadAliases() {
            ShowStatus('');
            $.ajax({
                url: 'api/audio/cardaliases',
                type: 'GET',
                dataType: 'json',
                success: function (data) {
                    audioCards = (data && data.cards) ? data.cards : [];
                    RenderAliases();
                },
                error: function (xhr) {
                    ShowStatus('Failed to load sound cards', true);
                    $('#aliasTableContainer').html(
                        '<div style="padding:1rem;color:#dc3545;">Failed to load sound cards.</div>'
                    );
                }
            });
        }

        function SaveAliases() {
            var aliases = {};
            $('#aliasTableContainer .alias-input').each(function () {
                var cardId = $(this).data('card-id');
                var val = $.trim($(this).val());
                if (cardId && val) {
                    aliases[cardId] = val;
                }
            });
            ShowStatus('Saving…');
            $.ajax({
                url: 'api/audio/cardaliases',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify({ aliases: aliases }),
                dataType: 'json',
                success: function (data) {
                    if (data && data.status === 'ok') {
                        ShowStatus('Saved ' + (data.count || 0) + ' alias(es)');
                        // Refresh underlying card data so newly-stale entries appear correctly
                        ReloadAliases();
                    } else {
                        ShowStatus(
                            'Save failed: ' + ((data && data.message) ? data.message : 'unknown error'),
                            true
                        );
                    }
                },
                error: function (xhr) {
                    var msg = 'Save failed';
                    try {
                        var resp = JSON.parse(xhr.responseText);
                        if (resp && resp.message) msg = 'Save failed: ' + resp.message;
                    } catch (e) { }
                    ShowStatus(msg, true);
                }
            });
        }

        $(document).ready(function () {
            ReloadAliases();
        });
    </script>
</body>

</html>
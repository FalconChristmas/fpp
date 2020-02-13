<?
$simplifiedPlaylist = 0;
?>

<div id="playlistDetails"  class="unselectable">
    <div id= "playlistDetailsContents">
        <table id="tblPlaylistDetails" width="100%">
            <colgroup>
                <col class='colPlaylistNumber'></col>
                <col class='colPlaylistData1'></col>
            </colgroup>
<?  if (!$simplifiedPlaylist) { ?>
            <tbody id='tblPlaylistLeadInHeader' style='display: none;' class='tblPlaylistDetailsHeader'>
                <tr><th colspan=4>-- Lead In -- (<span class='playlistItemCountWithLabelLeadIn'>0 items</span>, <span class='playlistDurationLeadIn'>00m:00s</span>)</th></tr>
            </tbody>
            <tbody id="tblPlaylistLeadIn" class='playlistEntriesBody'>
            </tbody>
<? } ?>
            <tbody id='tblPlaylistMainPlaylistHeader' style='display: none;' class='tblPlaylistDetailsHeader'>
                <tr><th colspan=4>-- Main Playlist -- (<span class='playlistItemCountWithLabelMainPlaylist'>0 items</span>, <span class='playlistDurationMainPlaylist'>00m:00s</span>)</th></tr>
            </tbody>
            <tbody id="tblPlaylistMainPlaylist" class='playlistEntriesBody'>
            </tbody>
<?  if (!$simplifiedPlaylist) { ?>
            <tbody id='tblPlaylistLeadOutHeader' style='display: none;' class='tblPlaylistDetailsHeader'>
                <tr><th colspan=4>-- Lead Out -- (<span class='playlistItemCountWithLabelLeadOut'>0 items</span>, <span class='playlistDurationLeadOut'>00m:00s</span>)</th></tr>
            </tbody>
            <tbody id="tblPlaylistLeadOut" class='playlistEntriesBody'>
            </tbody>
<? } ?>
        </table>
    </div>
</div>

<?
$simplifiedPlaylist = 0;
?>
<?  if (!$simplifiedPlaylist) { ?>
<? } ?>
<div id="playlistDetails"  class="unselectable">
    <div id= "playlistDetailsContents">
        <div id="tblPlaylistDetails" width="100%">
            <div>
                <div class='colPlaylistNumber'></div>
                <div class='colPlaylistData1'></div>
            </div>
<?  if (!$simplifiedPlaylist) { ?>
            <div class="tblPlaylistLeadInContainer tblPlaylistContainer">
                <div id='tblPlaylistLeadInHeader' style='display: none;' class='tblPlaylistDetailsHeader'>
                    <div>
                        <h2>Lead In</h2>
                        <div class="tblPlaylistHeaderDetails">
                            <div>
                                <div class="labelHeading">Items</div>
                                <div class="labelValue"><span class='playlistItemCountWithLabelLeadIn'>0</span></div>
                            </div>
                            <div>
                                <div class="labelHeading">Duration</div>
                                <div class="labelValue"><span class='playlistDurationLeadIn'>00m:00s</span></div>
                            </div>
                        </div>
                    </div>
                </div>
                <div class="tblPlaylistWrap">
                <table id="tblPlaylistLeadIn" class='playlistEntriesBody'>
                </table>
                </div>
            </div>
<? } ?>
            <div class="tblPlaylisttMainPlaylistContainer tblPlaylistContainer">
                <div id='tblPlaylistMainPlaylistHeader' style='display: none;' class='tblPlaylistDetailsHeader'>
                    <div>
                        <h2>Main Playlist</h2>
                        <div class="tblPlaylistHeaderDetails">
                            <div>
                                <div class="labelHeading">Items</div>
                                <div class="labelValue">
                                    <span class='playlistItemCountWithLabelMainPlaylist'>0</span>
                                </div>
                            </div>
                            <div>
                            	<div class="labelHeading">Duration</div>
                            	<div class="labelValue">
                            	 <span class='playlistDurationMainPlaylist'>00m:00s</span>
                            	</div>
                            </div>
                            
                        </div>
                    </div>
                </div>
                <div class="tblPlaylistWrap">
                    <table id="tblPlaylistMainPlaylist" class='playlistEntriesBody'>
                    </table>
                </div>
            </div>
<?  if (!$simplifiedPlaylist) { ?>
            <div class="tblPlaylistLeadOutContainer tblPlaylistContainer">
                <div id='tblPlaylistLeadOutHeader' style='display: none;' class='tblPlaylistDetailsHeader'>
                    <div><h2>Lead Out</h2> 
                        <div class="tblPlaylistHeaderDetails">
                            <div>
                                <div class="labelHeading">Items</div>
                                <div class="labelValue">
                                    <span class='playlistItemCountWithLabelLeadOut'>0</span>
                                </div>
                            </div>
                            <div>
                                <div class="labelHeading">Duration</div>
                                <div class="labelValue">
                                 <span class='playlistDurationLeadOut'>00m:00s</span>
                                </div>
                            </div>
                            
                        </div>
                    </div>
                </div>
                <div class="tblPlaylistWrap">
                <table id="tblPlaylistLeadOut" class='playlistEntriesBody'>
                </table>
                </div>
            </div>
<? } ?>
        </div>
    </div>
</div>

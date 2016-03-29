<template>
    <div id="playlists">
        <form>
            <div id="playlist-list" class="box panel">
                <div class="panel-heading">
                    <div class="panel-title">Playlists</div> 
                    <div class="panel-controls"><a href="#" class="button btn tiny gradient m-a-0" @click.prevent="newPlaylist">+ Add Playlist</a></div>
                </div>
                
                    <div class="table-responsive">
                        <table class="table table-sm table-striped table-hover pl-list m-b-0">
                            <thead>
                                <tr>
                                    <th>Name</th>
                                    <th class="text-center">Entries</th>
                                    <th>Next Scheduled</th>
                                </tr>
                            </thead>
                            <tbody>
                             <tr v-for="playlist in playlists" @click.prevent="editPlaylist($index)" :class="{'active' : playlist == activePlaylist}">
                                <td>{{ playlist.name}}</td>
                                <td class="text-center">{{ playlist.entries.length }}</td>
                                <td>{{ playlist.next_play }}</td>
                             </tr>
                        </table>
                    </div>
            </div>
             <div id="playlist-details" class="box p-a-1">
               <h4>
                <span class="semi-bold">Playlist</span> Details 
                    <div class="pull-xs-right"><button v-show="activePlaylist" class="button btn tiny gradient">Add Entry</button></div>
                </h4>  
                 <hr>
                <div class="playlist-info" v-if="hasPlaylist">
                    <div class="row">
                        <div class="col-xs-12 col-md-6">
                            <div class="form-group-default">

                                <label>Playlist Name</label>
                                <input type="text" v-model="activePlaylist.name" class="form-control">
                            </div>
                        </div>
                        <div class="col-md-6">
                            <div class="row">
                                <div class="col-md-3">
                                    <div class="form-group-default">
                                        <label>Repeat Playlist</label>
                                        <div class="p-t-5">
                                            <switch :model.sync="activePlaylist.repeat"></switch>
                                        </div>
                                    </div>
                                </div>
                                <div class="col-md-4">
                                     <div class="form-group-default" v-show="activePlaylist.repeat">
                                        <label>Loop Count</label>
                                        <input type="text" v-model="loopCount" class="form-control">
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="row">
                        <div class="col-xs-12 p-a-1">
                            
                        </div>
                    </div>
                    <div class="row">
                        <div class="col-xs-12 p-t-2">
                            <div class="row">
                                <div class="col-xs-12">
                                     <h4><span class="semi-bold">Playlist</span> Entries</h4>
                                     <hr>
                                </div>
                            </div>
                           
                            <div class="lead-in-section entry-section row m-b-2">
                                <div class="entry-section-labels col-xs-12 col-md-2">
                                    <h5 class="semi-bold m-b-0">Lead-in Items</h5>
                                    <p class="text-muted">These items will only be played during the first loop or initial play of this playlist.</p>   
                                </div>
                                
                                <div class="lead-in-items entry-items col-xs-12 col-md-10">
                                    <div id="lead-in-items" class="items-wrapper">
                                        <div class="add-entry"><i class="ion-ios-plus-outline"></i> Add Lead In Entry</div>
                                    </div>
                                </div>
                            </div>
                            <div class="primary-section entry-section m-b-2  row">
                                <div class="entry-section-labels col-xs-12 col-md-2">
                                    <h5 class="semi-bold m-b-0">Primary Items</h5>
                                    <p class="text-muted">These are the main items in your playlist, these items are played during each loop of the playlist.</p>
                                </div>
                                <div class="primary-items entry-items col-xs-12 col-md-10">
                                    <div id="primary-items" class="items-wrapper">
                                        <playlist-entry v-for="entry in activePlaylist.entries" :entry="entry"></playlist-entry>
                                        <div class="add-entry" @click.prevent="$broadcast('show::modal', 'entry-modal')"><i class="ion-ios-plus-outline"></i> Add Primary Entry</div>
                                    </div>
                                </div>
                            </div>
                            <div class="lead-out-section entry-section m-b-2  row">
                            <div class="entry-section-labels col-xs-12 col-md-2">
                                <h5 class="semi-bold m-b-0">Lead Out Items</h5>
                                <p class="text-muted">These items will be played once, at the end of your show.</p>
                            </div>
                                <div class="lead-out-items entry-items col-xs-12 col-md-10">
                                    <div id="lead-out-items" class="items-wrapper">
                                        <div class="add-entry"><i class="ion-ios-plus-outline"></i> Add Lead Out Entry</div>
                                    </div>
                                </div>
                            </div>
                            
                        </div>
                    </div>
                </div>
                <div class="no-playlist" v-show="!hasPlaylist">
                    <p class="text-center">Please select or add a playlist above</p>
                </div>
            </div>
        </form>
        <modal :types="getEntryTypes()"></modal>
    </div>    
</template>

<script>
import moment from "moment";
import Switch from "./shared/switch.vue";
import Sortable from "sortablejs";
import Modal from "./playlists/entry-modal.vue";
import PlaylistEntry from "./playlists/playlist-entry.vue";

const entryTypes = {
    'both'         : 'Media & Sequence',
    'media'        : 'Media Only',
    'sequence'     : 'Sequence Only',
    'pause'        : 'Pause',
    'event'        : 'Event',
    'effect'       : 'Effect',
    'plugin'       : 'Plugin',
    'playlist'     : 'Playlist',
    'pixelOverlay' : 'Pixel Overlay',
    'volume'       : 'Volume'
}

export default {
    components: { Switch, Modal, PlaylistEntry },
    props: [],
    data() {
        return {
            activePlaylist: null,
            playlists: [
                {   
                    name: 'Main', 
                    next_play: moment().add(2, 'days').format('h:mm a on ddd, MMM Do YYYY'),
                    repeat: 1,
                    loopCount: 3,
                    leadIn: [],
                    entries: [
                        {
                            type: 'both',
                            sequence: 'GriswoldIntro.fseq',
                            media: 'Griswold Hallelujah Intro 2011.mp3'
                        },
                        {
                            type: "both",
                            sequence: "Vacation.fseq",
                            media: "01 - Christmas Vacation.mp3"
                        },
                         {
                            type: "pause",
                            duration: 60,
                            playOnce: true,
                        },
                        {
                            type: "both",
                            sequence: "Nutrocker.fseq",
                            media: "NutrockerShortened.mp3"
                        },
                        {
                            type: "both",
                            sequence: "Frosty.fseq",
                            media: "Frosty The Snowman.mp3"
                        },
                         {
                            type: "volume",
                            volume: 75,
                        },
                        {
                            type: "both",
                            sequence: "WIW-Import1.fseq",
                            media: "04 - Wizards In Winter.mp3"
                        },
                        {
                            "type": "event",
                            "playOnce": 0,
                            "majorID": 3,
                            "minorID": 2,
                            "blocking": 0
                        },
                        {
                            type: "both",
                            sequence: "Grinch.fseq",
                            media: "GrinchShortened.mp3"
                        },
                        {
                            type: "both",
                            sequence: "Sarajevo.fseq",
                            media: "08 - Christmas Eve - Sarajevo 12.24.mp3"
                        }
                    ],
                    leadOut: [],

                }
            ]
        };
    },
    ready() {
        this._sortables = new Map();
    },
    methods: {
        editPlaylist(id) {
            this.activePlaylist = this.playlists[id];
            this.$nextTick(() => {
                this.createSortableList();
            });
        },
        getMediaColumn(entry) {
            if(entry.type == 'p') {
                return '<span class="label label-info">Pause</span> '+ entry.pause;
            } 
            if(entry.type == 'e') {
                return entry.event;
            }
            if(entry.type == 's' || entry.type == 'P') {
                return '---';
            }
            return entry.media;
        },
        getSequenceColumn(entry) {
           
            if(entry.type == 'P') {
                return entry.plugin;
            }
            if(entry.type == 'p' || entry.type == 'm' || entry.type == 'e') {
                return '---';
            }
            return entry.sequence;

        },
        getEntryTypes() {
            return entryTypes;
        },
        getEntryType(type) {
            return entryTypes[type];
        },
        removeEntry(entry) {
            console.log(entry);
            this.activePlaylist.entries.$remove(entry);
        },
        newPlaylist() {
            let playlist = {
                name: null,
                first_once: 0, 
                last_once: 0,
                next_play: null,
                entries: []
            }
            let id = this.playlists.push(playlist);
            this.editPlaylist(id - 1);
        },
        createSortableList() {

            // if(this.sortables && this.sortables.length) {
            //     for(sortable in this.sortables) {
            //         sortable.destroy();
            //     }
            //     //this.sortable.destroy();
            // }
            for(let sortable of this._sortables.values()) {
                sortable.destroy();
            }
            
            ['lead-in', 'primary', 'lead-out'].forEach(wrapper => {
                let id = wrapper + '-items';
                console.log(id);
                let el = document.getElementById(id);
                let group = wrapper == 'primary' ? 'entries' : wrapper.replace(/-([a-z])/g, g => g[1].toUpperCase() );
                
                let sortable = Sortable.create(el, {
                        draggable: '.playlist-entry',
                        animation: 150,
                        onUpdate: evt => {
                            let active = this.activePlaylist;
                            let content = active[group][evt.oldIndex];

                            active.entries.splice( evt.oldIndex, 1 );
                            active.entries.splice( evt.newIndex, 0, content );
                        }
                    });
                this._sortables.set(wrapper, sortable);

                
            });
            // this.sortable = Sortable.create(el, {
            //     draggable: '.playlist-entry',
            //     animation: 150,
            //     onUpdate: evt => {
            //         let active = this.activePlaylist;
            //         let content = active.entries[evt.oldIndex];

            //         active.entries.splice( evt.oldIndex, 1 );
            //         active.entries.splice( evt.newIndex, 0, content );
            //     }
            // });
        },
        addEntry(entry) {
            this.activePlaylist.entries.push(entry);
        }
    },
    computed: {
        hasPlaylist() { 
            return this.activePlaylist !== null;
        }
    },
    events: {
        'add-entry' : function(entry) {
            this.addEntry(entry);
        }
    }

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";
    $entry_colors : (
        both : #2b6a94,
        media : #3C93CE,
        sequence: #4A90E2,
        pause: #F8D053,
        event: #66AE12,
        pixel-overlay: #0DAD9E,
        volume: #F77975,
    );

    .pl-list {

        tr {
            cursor: pointer;
            td:first-child {
                padding-left: 18px;
            }
            td:last-child {
                padding-right: 18px;
            }
            &.active {
                td {
                    background: #daeffd !important;
                }
            }
        }
    }
    .playlist-entry {
        cursor: move;
        &.sortable-ghost {
            opacity: 0.6;
            td {
                background-color: #daeffd !important;
                //font-weight: bold;
            }
        }
        .remove {
            &:hover {
                color: $color-danger;
            }
        }

    }
    .entry-section {
        // padding-left: 20px;
        // border-left: 2px solid $secondary-color;
        clear: both; 

        .entry-section-labels {
            padding-right: 10px;
            border-right: 2px solid $secondary-color;
            // max-width: 125px;
            // float: left;
        }

        .entry-items {
            padding-left: 10px;
            // float: left;
        }
    }
    .items-wrapper {
        .add-entry {
            height: 57px;
            max-width: 550px;
            border: 1px dotted #d9d9d9;
            text-align: center;
            color: $primary-color;
            display: flex;
            align-items: center;
            justify-content: center;
            flex-direction: row;
            cursor: pointer;
            font-weight: 300;
            letter-spacing: 1px;
            i {
                margin-right: 10px;
                font-size: 24px;
            }
        }
        .playlist-entry {
            display: block;
            position: relative;
            padding: 5px;
            background: #F1F1F1;
            border: 1px solid #d9d9d9;
            max-width: 550px;
            margin-bottom: 5px;
            &:before {
                position: absolute;
                content: '';
                width: 5px;
                height: calc(100% - 10px);
                top: 5px;
                left: 5px;

            }
            .meta-row {
                font-size: 8px;
                color: $primary-color;
                text-transform: uppercase;
                letter-spacing: 2px;
                text-align: right;
                line-height: 1;
                margin-bottom: 2px;

                .entry-type {
                    // display: inline-block;
                    // padding: .4em 1em;
                    // border-radius: 1em;

                }
            }
            .media {
                display: block;
                margin-bottom: 0;
            }
            .main-row {
                display: flex;
                flex-direction: row;
                justify-content: space-between;
                align-items: center;
                padding-left: 20px;
                 
                 > div {
                    
                    &:first-child {
                        width: 45%;
                    }
                    &:last-child {
                        margin-left: auto;
                    }
                    .title {
                        white-space:nowrap;
                        text-overflow: ellipsis;
                        max-width: 220px;
                        overflow: hidden;   
                    }
                    
                 }
                // &:nth-child(2) {
                //     flex: 1 1 45%;
                // }
                // .actions {
                //     flex: 0 0 10%;
                // }
            }
            .title {
                font-weight: bold;
                font-size: 14px;
                line-height: 1;
            }
            .entry-label {
                font-size: 12px;
                color: $primary-color;
            }
            
            &.type-event .main-row {

                .major-id,
                .minor-id,
                .blocking {
                    width: 22.5%;
                }

            }
            @each $type, $color in $entry_colors {
                 &.type-#{$type} {
                    // .entry-type {
                    //     background-color: $color;
                    // }
                    &:before {
                        background-color: $color;
                    }
                 }           
            }
        }
    }
</style>
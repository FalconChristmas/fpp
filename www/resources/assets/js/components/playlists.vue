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
                    <div class="pull-xs-right"><button v-show="activePlaylist" class="button btn tiny gradient" @click.prevent="$broadcast('show::modal', 'entry-modal')">Add Entry</button></div>
                </h4>  
                 <hr>
                <div class="playlist-info" v-if="hasPlaylist">
                    <div class="row">
                        <div class="col-md-6">
                            <div class="form-group-default">

                                <label>Playlist Name</label>
                                <input type="text" v-model="activePlaylist.name" class="form-control">
                            </div>
                        </div>
                        <div class="col-md-6">
                            <div class="row">
                                <div class="col-md-6">
                                    <div class="form-group-default">
                                        <label>Play first entry only once</label>
                                        <div class="p-t-5">
                                            <switch :model.sync="activePlaylist.first_once"></switch>
                                        </div>
                                    </div>
                                </div>
                                <div class="col-md-6">
                                     <div class="form-group-default">
                                        <label>Play last entry only once</label>
                                         <div class="p-t-5">
                                            <switch :model.sync="activePlaylist.last_once"></switch>
                                        </div>
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
                            <h5><span class="semi-bold">Playlist</span> Entries</h5>
                            <div class="table-responsive">
                                <table class="table table-sm table-striped">
                                    <thead>
                                        <tr>
                                            <th></th>
                                            <th>Type</th>
                                            <th>Media / Event</th>
                                            <th>Sequence / Delay / Data</th>
                                            <th>Play First</th>
                                            <th>Play Last</th>
                                            <th></th>
                                        </tr>
                                    </thead>
                                    <tbody id="playlist-entry-list">
                                        <tr v-for="entry in activePlaylist.entries" id="entry-{{$index}}" data-index="{{$index}}" class="playlist-entry">
                                             <td class="p-l-10">{{ $index + 1 }}</td>
                                             <td>{{ getEntryType(entry.type) }}</td>
                                             <td>{{{ getMediaColumn(entry) }}}</td>
                                             <td>{{ getSequenceColumn(entry) }}</td>
                                             <td></td>
                                             <td></td>
                                             <td class="p-r-20"><a href="#" @click="removeEntry(entry)" title="Remove Entry" class="remove"><i class="ion-close"></i></td>
                                        </tr>
                                    </tbody>
                                </table>
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

const entryTypes = {
    'b' : 'Media & Sequence',
    'm' : 'Media Only',
    's' : 'Sequence Only',
    'p' : 'Pause',
    'e' : 'Event',
    'P' : 'Plugin',
}

export default {
    components: { Switch, Modal },
    props: [],
    data() {
        return {
            activePlaylist: null,
            playlists: [
                {   
                    name: 'Main', 
                    first_once: 0, 
                    last_once: 0,
                    next_play: moment().add(2, 'days').format('h:mm a on ddd, MMM Do YYYY'),
                    entries: [
                        {
                            type: 'b',
                            sequence: 'GriswoldIntro.fseq',
                            media: 'Griswold Hallelujah Intro 2011.mp3'
                        },
                        {
                            type: "b",
                            sequence: "Vacation.fseq",
                            media: "01 - Christmas Vacation.mp3"
                        },
                        {
                            type: "b",
                            sequence: "Nutrocker.fseq",
                            media: "NutrockerShortened.mp3"
                        },
                        {
                            type: "b",
                            sequence: "Frosty.fseq",
                            media: "Frosty The Snowman.mp3"
                        },
                        {
                            type: "b",
                            sequence: "WIW-Import1.fseq",
                            media: "04 - Wizards In Winter.mp3"
                        },
                        {
                            type: "b",
                            sequence: "Grinch.fseq",
                            media: "GrinchShortened.mp3"
                        },
                        {
                            type: "b",
                            sequence: "Sarajevo.fseq",
                            media: "08 - Christmas Eve - Sarajevo 12.24.mp3"
                        }
                    ]
                }
            ]
        };
    },
    ready() {

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
            if(this.sortable) {
                this.sortable.destroy();
            }
            let el = document.getElementById('playlist-entry-list');
            this.sortable = Sortable.create(el, {
                draggable: '.playlist-entry',
                animation: 150,
                onUpdate: evt => {
                    let active = this.activePlaylist;
                    let content = active.entries[evt.oldIndex];

                    active.entries.splice( evt.oldIndex, 1 );
                    active.entries.splice( evt.newIndex, 0, content );
                }
            });
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
</style>
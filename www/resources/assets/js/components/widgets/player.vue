<template>
   <div class="box panel widget widget-player">
        <div class="panel-heading">
            <div class="panel-title">Player</div>
            <div class="panel-controls">
                <select name="" class="form-control c-select-sm c-select" id="">
                    <option value="">Select a Playlist</option>
                </select>
          </div>
        </div>
        <div class="panel-body p-a-0">
            <div class="player-items">
                <div class="table-responsive">
                    <table class="table table-sm table-striped m-t-0">
                        <thead>
                            <tr>
                                <th></th>
                                <th>Media</th>
                                <th>Sequence</th>
                                <th class="text-center">Duration</th>
                                <th>Play Once</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr v-for="item in items"> 
                                <td class="play"><i class="ion-play"></i></td>
                                <td class="media-name">{{item.media}}</td>
                                <td class="sequence-name">{{item.sequence}}</td>
                                <td class="duration text-center">{{item.duration}}</td>
                                <td class="play-once"><span class="po-block po-start">S</span><span class="po-block po-end">E</span></td>
                            </tr>
                            <tr v-if="!items">
                                <td colspan="5" class="text-center all-caps fs-12 ls-2 text-muted p-a-1">No Playlist Selected</td>
                            </tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    </div>
</template>
<script>
export default {
    components: {},
    props: [],
    data() {
        return {
            items: [
            {media: 'THX Intro.mp3', sequence: 'Intro-1.fseq', duration: '0:35'},
            {media: 'Wizards in Winter.mp3', sequence: 'Wizards In Winder.fseq', duration: '3:13'},
            {media: 'Sarajevo.mp3', sequence: 'Sarajevo.fseq', duration: '4:23'},
            {media: 'Vacation.mp3', sequence: 'Chrismas Vacation.fseq', duration: '2:23'},
            {media: 'Frosty.mp3', sequence: 'Frosty-New.fseq', duration: '3:40'},
            {media: 'Nutrocker-Shortened.mp3', sequence: 'Nutrocker.fseq', duration: '4:23'},
            ]
        };
    },
    ready() {},
    methods: {},
    events: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";
    
.widget-player {
    .table {
        thead {
            th {
                font-size: 9px;
                background: #e6e6e6;//#b1b1b1;
                font-weight: normal;
                border-bottom: 0;
                //color: white;
            }
        }
    }
    .player-items {
        
        .table {
            tr {
                td {
                     vertical-align: middle;
                    &.play {
                        text-align: center;
                        padding-left: 10px;
                        padding-right: 10px;
                    }
                }
            }
        }
        .play-once {
            padding-right: 25px;
            .po-block {
                padding: 3px 8px;
                border: 1px solid #ddd;
                font-size: 12px;
                display: inline-block;
                margin: 0 3px;
                color: #ddd;
            }
        }
    }
}
</style>
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
                                <th>Duration</th>
                                <th>Play Once</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr>
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
        return {};
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
}
</style>
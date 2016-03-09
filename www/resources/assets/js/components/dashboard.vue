<template>
    <div id="dashboard" class="container-fluid">
        <div class="row">
            <div class="col-xs-12 col-lg-7">
                <status-widget></status-widget>
            </div>
           
            <div class="col-xs-12 col-lg-5">
                <div class="row">
                    <div class="col-xs-12 col-md-6 col-lg-12">
                        
                        <div class="panel box widget bg-azure">
                             <span class="widget-bg-icon">
                                    <i class="ion-clock fs-20"></i>
                                </span>
                            <div class="panel-heading">
                                <div class="panel-title text-white hint-text">FPP Time</div>
                            </div>
                            <div class="panel-body">
                                <h4 class="text-lg m-a-0 text-white">{{ time }}</h4>
                                <h6 class="hint-text m-a-0 text-white">{{ date }}</h6>
                               
                            </div>
                        </div>
                      
                    </div>
                    <div class="col-xs-12 col-md-6 col-lg-12">
                        <div class="box widget p-a-1 col-xs-6 col-lg-12">
                           <!--  <div class="row">
                                <div class="col-xs-1"> -->
                                    <span class="pull-xs-left m-r-1 rounded bg-info widget-icon">
                                        <i class="ion-calendar"></i>
                                   </span>
                                <!-- </div> --> 
                                <div class="clear">

                                    <h4 class="m-a-0">Next <span class="semi-bold">Playlist</span> <span class="pull-xs-right">6:00 pm</span></h4>
                                      
                                    <h6 class="m-a-0 text-muted small-text "><span class="all-caps ls-1">Main Playlist</span> <span class="pull-xs-right">{{ next }}</span></h6>
                                </div>
                            <!-- </div> -->
                           
                        </div>
                    </div>
                </div>
                
            </div>
            
        </div>
        <div class="row">
            <div class="col-xs-12">
                <player-widget></player-widget>
            </div>
        </div>
        
    </div>
</template>

<script>
import moment from "moment";
import WeatherWidget from "./widgets/weather.vue";
import StatusWidget from "./widgets/status.vue";
import PlayerWidget from "./widgets/player.vue";

export default {
    components: { WeatherWidget, StatusWidget, PlayerWidget },
    props: [],
    data() {
        return {};
    },
    ready() {},
    methods: {},
    computed: {
        date() {
            return moment().format('ddd, MMM Do YYYY');
        },
        time() {
            return moment().format('h:mm a');
        },
        next() {
            return moment().add(2, 'days').format('ddd, MMM Do YYYY');
        }
    },
    events: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

</style>
<template>
    <div id="date-settings" class="settings-container container-fluid box p-a-2">
        <h4><span class="semi-bold">Date & Time</span> Settings</h4>     
        <hr>
        <form>
            
            <div class="form-group row">
                <div class="col-sm-6 col-md-8">
                    <label for="" class="form-control-label">Manually Set Date & Time</label>
                </div>
                <div class="col-sm-3 col-md-2 text-xs-right">
                    <input type="text" v-model="date" class="form-control" placeholder="YYYY/MM/DD">
                    
                </div>           
                <div class="col-sm-3 col-md-2 text-xs-right">
                    <input type="text" v-model="time" placeholder="HH:MM" class="form-control">
                    <small class="text-muted">Expected 24 hour time</small>
                </div> 
            </div>
            <div class="form-group row">
                <div class="col-sm-7">
                    <label for="" class="form-control-label">Real Time Clock</label>
                    <p class="text-muted  hidden-sm-down"><strong>Note:</strong> You must reboot to activate the RTC, then return to this page to set the time on the RTC.</p>
                </div>
                <div class="col-sm-2 col-sm-offset-3 text-xs-right">
                    <select class="c-select" v-model="piRTC">
                        <option value="N">None</option>
                        <option value="1">RasClock</option>
                        <option value="2">DS1305</option>
                        <option value="3">PiFace</option>
                    </select>
                </div>           
                
            </div>
             <div class="form-group row">
                <div class="col-sm-6 col-md-7">
                    <label for="" class="form-control-label">NTP</label>
                    <p class="text-muted  hidden-sm-down">This allows you to enable or disable the NTP (Network Time Protocol) functionality of the player. When enabled, the player will attempt to retrieve the current time from a predefined set of NTP servers OR from an NTP you specify.</p>
                </div>
                <div class="col-sm-2 text-xs-right">
                    <switch :model.sync="ntp"></switch>
                </div>           
                <div class="col-sm-3 text-xs-right" v-if="ntp">
                    <input type="text" v-model="ntp_server" placeholder="" class="form-control">
                    <small class="text-muted">NTP Server (optional)</small>
                </div> 
            </div>
            <div class="form-group row">
                <div class="col-sm-7">
                    <label for="" class="form-control-label text-md">Time Zone</label>
                    <p class="text-muted  hidden-sm-down">Please select your timezone.</p>
                </div>
                <div class="col-sm-2 col-sm-offset-1">
                    <select v-model="timezone" class="c-select col-sm-2 pull-sm-right">
                        <option v-for="(index, key) in timezones" value="{{index}}">{{key}}</option>
                    </select>
                </div>
                
                
            </div>
        </form>
    </div>
</template>

<script>
import Switch from "../../../shared/switch.vue";

export default {
    components: { Switch },
    props: [],
    data() {
        return {
            date: null,
            time: null,
            piRTC: null,
            ntp: true,
            ntp_server: null,
            timezone: null,
            timezones: []
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

</style>
<template>
    <div id="general-settings" class="settings-container container-fluid box p-a-2">   
        <h4><span class="semi-bold">General</span> Settings</h4>     
        <hr>
        <form>
            <div class="form-group row">
                <div class="col-sm-7">
                    <label for="" class="form-control-label">Blank Screen on Startup</label>
                    <p class="text-muted  hidden-sm-down">Enable/Disable a screen blanker at FPP startup. If you are using a projector with your Pi to display video then you will want to enable this setting so that the text console does not show on your projector when you are not playing videos</p>
                </div>
                <div class="col-sm-2 col-sm-offset-1 text-xs-right">
                    <switch :model.sync="blank_screen"></switch>
                </div>           
                
            </div>
            <div class="form-group row">
                <div class="col-sm-7">
                    <label for="" class="form-control-label text-md">Force Analog Audio Output</label>
                    <p class="text-muted  hidden-sm-down">Force audio output out the 1/8-inch stereo jack even when using HDMI for video</p>
                </div>
                <div class="col-sm-2 col-sm-offset-1 text-xs-right">
                     <switch :model.sync="force_analog"></switch>
                </div>
                
               
            </div>
            <div class="form-group row">
                <div class="col-sm-7">
                    <label for="" class="form-control-label text-md">Pi 2x16 LCD Enabled</label>
                    <p class="text-muted  hidden-sm-down">Enable use of a LCD display attache to the Pi for status and control</p>
                </div>
                <div class="col-sm-2 col-sm-offset-1 text-xs-right">
                     <switch :model.sync="lcd_enabled"></switch>
                </div>
            </div>
            <div class="form-group row">
                <div class="col-sm-7">
                    <label for="" class="form-control-label text-md">Always Transmit Channel Data</label>
                    <p class="text-muted  hidden-sm-down">Force transmission of channel data out to controllers whenever FPP is running. FPP will normally only transmit data when there is a sequence playing or the system is running in Bridge mode or a Pixel Overlay model is enabled. Some controllers go into test mode when not receiving data. This setting causes FPP to always send data so the controllers do not go into test mode.</p>
                </div>
                <div class="col-sm-2 col-sm-offset-1 text-xs-right">
                    <switch :model.sync="always_transmit"></switch>
                </div>
                
                
            </div>
            <div class="form-group row">
                 <div class="col-sm-7">
                    <label for="" class="form-control-label text-md">Audio Output Device</label>
                    <p class="text-muted  hidden-sm-down">Allows controlling whether audio is sent out via the onboard soundcard or a USB attached soundcard or FM transmitter.</p>
                </div>
                <div class="col-sm-2 col-sm-offset-1">
                     <select v-model="audio_device" class="c-select col-sm-2 pull-sm-right">
                        <option v-for="(index, key) in soundCards" value="{{index}}">{{key}}</option>
                    </select>
                </div>
                
               
            </div>
            <div class="form-group row">
                <div class="col-sm-7">
                    <label for="audio_mixer" class="form-control-label text-md">Audio Output Mixer Device</label>
                    <p class="text-muted  hidden-sm-down"></p>
                </div>
                <div class="col-sm-2 col-sm-offset-1">
                    <select v-model="audio_mixer" class="c-select col-sm-2 pull-sm-right">
                        <option v-for="(index, key) in mixerDevices" value="{{index}}">{{key}}</option>
                    </select>
                </div>
                
                
            </div>
        </form>
    </div>
</template>

<script>
import Switch from "../shared/switch.vue";
import vSelect from "vue-strap/src/Select.vue";
import vOption from "vue-strap/src/Option.vue";

export default {
    components: { Switch, vSelect, vOption },
    props: [],
    data() {
        return {
            blank_screen: true,
            always_transmit: false,
            force_analog: false,
            lcd_enabled: false,
            audio_device: [],
            soundCards: ['Alsa', 'HiFi Berry'],
            mixerDevices: ['DSP Program']
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
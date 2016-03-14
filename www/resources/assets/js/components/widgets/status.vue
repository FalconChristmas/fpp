<template>
    <div class="panel box fppd-status" data-pages="portlet">
        <div class="panel-heading separator">
            <div class="panel-title">FPPD</div>
            <div class="panel-controls">
                <ul>
                  <li><a href="#" class="portlet-collapse" data-toggle="collapse"><i class="portlet-icon portlet-icon-collapse"></i></a>
                  </li>
                  <li><a href="#" class="portlet-refresh" data-toggle="refresh"><i class="portlet-icon portlet-icon-refresh"></i></a>
                  </li>
                  <li><a href="#" class="portlet-close" data-toggle="close"><i class="portlet-icon portlet-icon-close"></i></a>
                  </li>
                </ul>
          </div>
        </div>
        <div class="panel-body">
            
            <div class="widget text-center current-status fppd-running">
                <span class="widget-header">Status</span>
                <span class="widget-icon"><i class="icon" :class="modeIconClass"></i></span>
                <span class="widget-text">{{ fppd | capitalize }}</span>
                <div class="widget-buttons">
                    <a href="#" class="button small secondary gradient fppd-stop" v-if="fppd == 'running'" @click="stopFPPD"><i class="icon ion-close"></i> Stop</a>
                    <a href="#" class="button small secondary gradient fppd-start" v-if="fppd == 'stopped'" @click="startFPPD"><i class="icon ion-checkmark"></i> Start</a>
                    <a href="#" class="button small secondary gradient fppd-restart" @click="restartFPPD"><i class="icon ion-refresh"></i> Restart</a>
                </div>
            </div>          

            <div class="widget text-center current-status">
                <span class="widget-header">Mode</span>
                <span class="widget-icon">
                    <span v-if="mode != 'stopped'" class="text-white rounded fs-18 bg-info-light p-t-5 p-b-5 p-l-10 p-r-10" style="vertical-align:middle">{{ mode.charAt(0) | capitalize }}</span>
                    <i v-if="mode == 'stopped'" class="icon ion-close-circled text-danger"></i>
                </span>
                <span class="widget-text">{{ mode | capitalize }}</span>
                <div class="widget-buttons">
                    <select v-model="newMode" class="fppd-mode-select c-select">
                        <option value="">-- FPPD Mode --</option>
                        <option value="standalone">Standalone</option>
                        <option value="master">Master</option>
                        <option value="remote">Remote</option>
                        <option value="bridge">Bridge</option>
                    </select>
                    <a class="button small secondary gradient fppd-mode-apply" @click="changeMode">Apply</a>
                </div>
            </div>
        </div>
        
    </div>
</template>

<script>
import {
    startFPPD,
    stopFPPD,
    restartFPPD,
    updateMode,
} from "../../state/actions";

export default {
    components: {},
    props: [],
    data() {
        return {
            newMode: ''
        };
    },
    ready() {},
    methods: {
        changeMode() {
            if(this.newMode != '' && this.newMode != this.mode) {
                this.updateMode(this.newMode);
            }
        }
    },
    events: {},
    computed: {
        modeIconClass() {
            return classes = this.fppd == 'running' ? ['ion-checkmark-circled', 'text-success'] : ['ion-close-circled', 'text-danger'];
        }
    },
    vuex: {
        getters: {
            status: state => state.shared.status,
            mode: state => state.shared.fppd == 'running' ? state.shared.mode : state.shared.fppd,
            fppd: state => state.shared.fppd
        },
        actions: {
            startFPPD,
            stopFPPD,
            restartFPPD,
            updateMode
        }
    }

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";
    
.fppd-status {
    box-shadow: 0 0 1px rgba(0, 0, 0, 0.15);
        .panel-heading {
            padding: 15px 20px 5px 20px;
        }
        .panel-body {
            margin-left: auto;
            margin-right: auto;
            padding: 0;
            lost-utility: clearfix;
        }
        .widget {
            color: $heading-color;
            letter-spacing: 1px;
            text-transform: uppercase;
            lost-column: 1/2 2 0;
        
            &:first-child {
                border-right: 1px solid $secondary-color;
            }

            &-header {
                font-size: 10px;
                display: block;
                text-transform: uppercase;
                padding: 8px 5px 5px;                
            }
            &-icon {
                font-size: 34px;
                line-height:28px;
                height: 35px;
                width: 35px;
                i {
                    font-size: 34px;
                }
                img {
                    width: 31px;
                }
            }
            &-text {
                display: block;
                padding: 5px 5px 8px;
                font-size: 12px;
                letter-spacing: .1rem;
            }

            &-buttons {
                border-top: 1px solid $secondary-color;
                background: lighten($secondary-color, 6%);
                padding: 10px;
                .button {
                    display: inline-block;
                    vertical-align: middle;
                    margin: 0 1%;
                    text-transform: uppercase;
                    letter-spacing: 1px;
                    font-size: 10px;
                    @include gradient-button();
                    i { margin-right: 3px; }
                    &:focus {
                        outline: none;
                    }
                }
                .form-group {
                    margin: 0;
                    display: inline-block;
                    width: 60%;
                    > select {
                        width: 100%;
                    }
                }
                select {
                    width: 60%;
                    height: auto;
                    display: inline-block;
                    padding: .09rem .25rem;
                    margin-bottom: 0;
                    background-color: transparent;
                    border-color: darken($secondary-color, 15%);
                    border-radius: 2px;
                    box-shadow: none;
                    line-height: inherit;
                    background: #fafafa url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" version="1.1" width="32" height="24" viewBox="0 0 32 24"><polygon points="0,0 32,0 16,24" style="fill: #000"></polygon></svg>') right 10px center no-repeat;
                    background-size: 8px 8px;
                    color: #888;
                    font-size: 13px;

                }
            }
            
        }

        .mode-icon {

            background: #788B94;
            color: white;
            padding: 3px 10px;
            border-radius: 35px;
            font-size: 20px;
            vertical-align: top;
            line-height: 35px;

        }
    }
</style>
<template>
    <div id="app" tabindex="0">        
        <sidebar></sidebar>
        <div class="content-wrapper">
           <router-view></router-view>
        </div>
        <overlay v-show="loading"></overlay>
    </div>
</template>

<script>
    import $ from 'jquery';
    
    import sidebar from './components/sidebar.vue';
    import mainContent from './components/index.vue';
    import overlay from './components/shared/overlay.vue';
    import store from "./state/store";
    import { 
        rebootDevice,
        shutdownDevice,
        startFPPD,
        stopFPPD,
        restartFPPD,
        requestStatus
    } from "./state/actions";   
   
    export default {
        store,
        components: { sidebar, mainContent, overlay },
        replace: false,
        data() {
            return {
                loading: false,
            };
        },

        ready() {
            this.toggleOverlay();
            
            // Make the most important HTTP request to get all necessary data from the server.
            // Afterwards, init all mandatory stores and services.
            this.initialize(() => {
                // Hide the overlaying loading screen.
                this.toggleOverlay();
                // Let all other components know we're ready.
                this.$broadcast('fpp:ready');
            });
           
        },

        methods: {

            /**
             * Show or hide the loading overlay.
             */
            toggleOverlay() {
                this.loading = !this.loading;
            }
        },
        vuex: {
            actions: {
                initialize: ({ dispatch }, cb = null) => {
                    Vue.http.get('status').then((response) => {
                        let data = response.data.data;
                        
                        dispatch('DEVICE_BOOTED');
                        dispatch('STATUS_UPDATE', data.status);
                        dispatch('UPDATE_MODE', data.mode);
                        dispatch('UPDATE_FPPD', data.fppd);
                        dispatch('UPDATE_VOLUME', data.volume);
                        dispatch('UPDATE_PLAYLIST', data.playlist);
                        dispatch('UPDATE_DATETIME', data.currentDate);
                        dispatch('UPDATE_VERSION', data.version);

                        if(cb) {
                            cb();
                        }
                    });
                },
                rebootDevice,
                shutdownDevice,
                startFPPD,
                stopFPPD,
                restartFPPD,
                requestStatus,
            }
        }
    };

</script>


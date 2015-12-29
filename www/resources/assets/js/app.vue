<template>
    <div id="app" tabindex="0" @keydown.f = "search">        
        <sidebar></sidebar>
        <main-content></main-content>
        <overlay v-show="loading"></overlay>
    </div>
</template>

<script>
    import $ from 'jquery';
    
    import sidebar from './components/sidebar/index.vue';
    import mainContent from './components/main/index.vue';
    import overlay from './components/shared/overlay.vue';

    import sharedStore from './stores/shared';
    import playlistStore from './stores/playlist';
    import settingStore from './stores/setting';
   
    export default {
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
            sharedStore.init(() => {
                this.initStores();
                // Hide the overlaying loading screen.
                //this.toggleOverlay();

                // Ask for user's notificatio permission.
                this.requestNotifPermission();

                // Let all other components know we're ready.
                this.$broadcast('fpp:ready');
            });
        },

        methods: {
            /**
             * Initialize all stores to be used throughout the application.
             */
            initStores() {
                // playlistStore.init();
                // settingStore.init();
            },
          

            /**
             * Load (display) a main panel (view).
             *
             * @param string view The view, which can be found under components/main/main-content.
             */
            loadMainView(view) {
                this.$broadcast('main-content-view:load', view);
            },

            /**
             * Load a playlist into the main panel.
             *
             * @param object playlist The playlist object
             */
            loadPlaylist(playlist) {
                this.$broadcast('playlist:load', playlist);
                this.loadMainView('playlist');
            },
          
            /**
             * Show or hide the loading overlay.
             */
            toggleOverlay() {
                this.loading = !this.loading;
            }
        },
    };

</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

    #app {
        display: flex;
        min-height: 100vh;
        flex-direction: column;
        
        background: $bg-color;
        color: $text-color;

        font-family: $font-primary;
        font-size: $font-size;
        line-height: $font-size * 1.5;
        
    }
</style>

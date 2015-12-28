<template>
    <div id="app" tabindex="0" 
        @keydown.f = "search"
    >
        
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
                this.toggleOverlay();

                // Ask for user's notificatio permission.
                this.requestNotifPermission();

                // Let all other compoenents know we're ready.
                this.$broadcast('fpp:ready');
            });
        },

        methods: {
            /**
             * Initialize all stores to be used throughout the application.
             */
            initStores() {
                playlistStore.init();
                settingStore.init();
            },

            /**
             * Play the prev song when user presses K.
             *
             * @param object e The keydown event
             */
            playPrev(e) {
                if ($(e.target).is('input,textarea')) {
                    return true;
                }

                playback.playPrev();
                e.preventDefault();
            },

            /**
             * Play the next song when user presses J.
             *
             * @param object e The keydown event
             */
            playNext(e) {
                if ($(e.target).is('input,textarea')) {
                    return true;
                }

                playback.playNext();
                e.preventDefault();
            },

            /**
             * Put focus into the search field when user presses F.
             *
             * @param object e The keydown event
             */
            search(e) {
                if ($(e.target).is('input,textarea')) {
                    return true;
                }

                $('#searchForm input[type="search"]').focus().select();
                e.preventDefault();
            },

            /**
             * Request for notification permission if it's not provided and the user is OK with notifs.
             */
            requestNotifPermission() {
                if (window.Notification && this.prefs.notify && Notification.permission !== 'granted') {
                    Notification.requestPermission(result => {
                        if (result === 'denied') {
                            preferenceStore.set('notify', false);
                        }
                    });
                }
            },

            /**
             * Load (display) a main panel (view).
             *
             * @param string view The view, which can be found under components/main-wrapper/main-content.
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

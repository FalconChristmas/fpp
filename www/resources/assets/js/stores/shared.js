import http from '../services/http';

export default {
    state: {
        fppd: {},
        settings: [],
        currentUser: null,
        playlists: [],
        schedule: {}
    },

    init(cb = null) {
        http.get('status', {}, data => {
            this.state.fppd = data.fppd;
            this.state.settings = data.settings;
            this.state.playlists = data.playlists;
            this.state.schedule = data.schedule;
            this.state.currentUser = data.user;
         

            if (cb) {
                cb();
            }
        });
    },
};

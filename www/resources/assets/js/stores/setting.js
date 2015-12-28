import http from '../services/http';
import sharedStore from './shared';

export default {
        
    state: {
        settings: [], 
    },
    
    init() {
        this.state.settings = sharedStore.state.settings;
    },

    all() {
        return this.state.settings;
    },

    update(cb = null) {
        http.post('settings', this.all(), msg => {
            if (cb) {
                cb();
            }
        });
    },
};

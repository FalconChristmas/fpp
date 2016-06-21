/**
 * Device Actions
 */
export const rebootDevice = ({ dispatch }) => dispatch('REBOOT_DEVICE');
export const shutdownDevice = ({ dispatch }) => dispatch('SHUTDOWN_DEVICE');
export const requireRestart = ({ dispatch }) => dispatch('REQUIRE_RESTART');

/**
 * FPPD Actions
 */
export const startFPPD = ({ dispatch }) => {
    // send ajax request to start
    dispatch('START_FPPD');
}

export const stopFPPD = ({ dispatch }) => {
    // send ajax request to stop
    dispatch('STOP_FPPD');
}

export const restartFPPD = ({ dispatch }) => {
    //send ajax request to restart
    dispatch('RESTART_FPPD');
}

export const updateMode = ({ dispatch, state }, mode) => {
    if(state.mode !== mode) {
        dispatch('UPDATE_MODE', mode);
        dispatch('REQUIRE_RESTART');
    }
}

/**
 * Status Actions
 */
export const requestStatus = ({ dispatch }) => {
    dispatch('STATUS_REQUEST');
    Vue.http.get('status').then(data => {
        dispatch('STATUS_UPDATE', data.status);
        dispatch('UPDATE_MODE', data.mode);
        if(data.fppd != 'running') dispatch('STOP_FPPD');
    });
};

export const updateStatus = ({ dispatch }, status) => dispatch('STATUS_UPDATE', status);

/**
 * Output Actions
 */
export const toggleOutputs = ({ dispatch }, type, value) => dispatch('TOGGLE_OUTPUTS', type, value);
export const addOutput = ({ dispatch }, type, output) => dispatch('ADD_OUTPUT', type, output);
export const removeOutput = ({ dispatch }, type, id) => dispatch('REMOVE_OUTPUT', type, id);
export const updateOutput = ({ dispatch }, type, output) => dispatch('REMOVE_OUTPUT', type, output);
export const getAllOutputs = ({ dispatch }) => {
    Vue.http.get('outputs').then(data => {
        dispatch('RECEIVE_OUTPUTS', data.data);
    });

}

/**
 * Playlist Actions
 */
export const getAllPlaylists = ({ dispatch }) => {
    Vue.http.get('playlists').then(data => {
        dispatch('RECEIVE_PLAYLISTS', data.data);
    });
}
export const createPlaylist = ({ dispatch }, playlist) => dispatch('NEW_PLAYLIST', playlist);
export const deletePlaylist = ({ dispatch }, playlist) => dispatch('DELETE_PLAYLIST', playlist);
export const updatePlaylist = ({ dispatch }, playlist) => dispatch('UPDATE_PLAYLIST', playlist);

/**
 * Event Actions
 */
export const triggerEvent = ({ dispatch }, event) => dispatch('TRIGGER_EVENT', event);
export const addEvent = ({ dispatch }, event) => dispatch('ADD_EVENT', event);
export const saveEvent = ({ dispatch }, event) => dispatch('SAVE_EVENT', event);
export const editEvent = ({ dispatch }, event) => dispatch('EDIT_EVENT', event);
export const deleteEvent = ({ dispatch }, event) => dispatch('DELETE_EVENT', event);

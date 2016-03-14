import http from '../services/http';


/**
 * Device Actions
 */
export const rebootDevice = ({ dispatch }) => dispatch('REBOOT_DEVICE');
export const shutdownDevice = ({ dispatch }) => dispatch('SHUTDOWN_DEVICE');
export const requireRestart = ({ dispatch }) => dispatch('REQUIRE_RESTART');

/**
 * FPPD Actions
 */
export const startFPPD = ({ dispatch }) => dispatch('START_FPPD');
export const stopFPPD = ({ dispatch }) => dispatch('STOP_FPPD');
export const restartFPPD = ({ dispatch }) => dispatch('RESTART_FPPD');
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


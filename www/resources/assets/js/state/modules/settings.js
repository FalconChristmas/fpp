import {
    RECEIVE_SETTINGS,
} from "../mutation-types";

const state = {
    general: {
        alwaysTransmit: true,
        forceHDMI: false,
        screensaver: true,
        forceLocalAudio: false,
        piLCDEnabled: false,
        audioOutput: '',
        audioMixerDevice: '',
        storageDevice: ''
    },
    datetime: {},
    email: {},
    network: {},
    logs: {},
    advanced: {},
    ui: {},
}

const mutations = {
      [RECEIVE_SETTINGS] (state, settings) {
        Object.assign(state, settings);
    },
}

export default {
  state,
  mutations
}
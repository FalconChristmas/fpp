import {
    DEVICE_BOOTED,
    REBOOT_DEVICE,
    REQUIRE_REBOOT,
    SHUTDOWN_DEVICE,
    START_FPPD,
    STOP_FPPD,
    RESTART_FPPD,
    REQUIRE_RESTART,
    STATUS_REQUEST,
    STATUS_UPDATE,
    UPDATE_VOLUME,
    UPDATE_PLAYLIST,
    UPDATE_MODE
} from "../mutation-types.js"


const state = {
    fppd: "running",
    mode: "standalone",
    status: 0,
    volume: 0,
    playlist: "No playlist scheduled.",
    currentDate: {
        date: "2016-03-10 19:06:47.000000",
        timezone_type: 3,
        timezone: "UTC"
        },
    repeatMode: 0,
    deviceBooted: false,
    restartFlag: false,
    rebootFlag: false,
    updateFlag: false,
    rebooting: false,
    shuttingDown: false
};


const mutations = {

    [REQUIRE_REBOOT] (state) {
        state.rebootFlag = true;
    },

    [REQUIRE_RESTART] (state) {
        state.restartFlag = true;
    },

    [REBOOT_DEVICE] (state) {
        state.rebootFlag = false;
        state.rebooting = true;
    },
    
    [DEVICE_BOOTED] (state) {
        state.deviceBooted = true;
        state.rebooting = false;
        state.shuttingDown = false;
    },

    [SHUTDOWN_DEVICE] (state) {
        state.rebootFlag = false;
        state.shuttingDown = true;
    },

    [START_FPPD] (state) {
        state.fppd = 'running';
    },

    [STOP_FPPD] (state) {
        state.fppd = 'stopped';
        state.restartFlag = false;
    },

    [UPDATE_MODE] (state, mode) {
        state.mode = mode;
    },

    [STATUS_UPDATE] (state, status) {
        state.status = status;
    }, 

    [UPDATE_VOLUME] (state, volume) {
        state.volume = volume;
    },

    [UPDATE_PLAYLIST] (state, playlist) {
        state.playlist = playlist;
    },

}

export default {
  state,
  mutations
}

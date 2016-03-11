import {
    ADD_OUTPUT,
    REMOVE_OUTPUT,
    UPDATE_OUTPUT,
    RECEIVE_OUTPUTS,
    TOGGLE_OUTPUTS
} from "../mutation-types.js"

const state = {
    e131: {
        enabled: false,
        outputs: [],
    },
    pdmx: {
        enabled: false,
        outputs: [],
    },
    panels: {
        enabled: false,
        outputs: [],
    },
    bbb: {
        enabled: false,
        outputs: [],
    },
    lor: {
        enabled: false,
        outputs: [],
    },
    serial: {
        enabled: false,
        outputs: [],
    },
    renard: {
        enabled: false,
        outputs: [],
    },
    pixelnet: {
        enabled: false,
        outputs: [],
    },
    triks: {
        enabled: false,
        outputs: [],
    },
    virtual: {
        enabled: false,
        outputs: []

},}

const mutations = {
    [TOGGLE_OUTPUTS] (state, type, value) {
        state[type].enabled = value;
    },
    [ADD_OUTPUT] (state, type, output) {
        state[type].push(output);
    },
    [REMOVE_OUTPUT] (state, type, id) {
        state[type].splice(id, 1);
    },
    [UPDATE_OUTPUT] (state, type, id, output) {
        state[type][id] = output;
    },
    [RECEIVE_OUTPUTS] (state, outputs) {
      
        Object.assign(state, outputs);
        
    },
}

export default {
  state,
  mutations
}
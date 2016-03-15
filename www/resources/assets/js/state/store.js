import Vue from 'vue'
import Vuex from 'vuex'
import shared from './modules/shared'
import outputs from './modules/outputs'
import settings from "./modules/settings";

Vue.use(Vuex)

export default new Vuex.Store({
  modules: {
    shared,
    outputs,
    settings
  }
})



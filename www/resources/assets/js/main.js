window.Vue = require('vue');

var $ = require('jquery');
var VueValidator = require('vue-validator');

Vue.use(VueValidator);
Vue.use(require('vue-animated-list'));
Vue.use(require('vue-resource'));

Vue.http.options.root = '/api';
Vue.http.headers.common['X-CSRF-TOKEN'] = $('meta[name="csrf-token"]').attr('content');

Vue.config.debug = true;

var App = require('./app.vue');
var router = require('./router');

/**
 * Get After It..
 */
router.start(App, 'body');

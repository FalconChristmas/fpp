var $ = require('jquery');
var VueRouter = require('vue-router');

window.Vue = require('vue');
Vue.use(VueRouter);
Vue.use(require('vue-resource'));
Vue.http.options.root = '/api';
Vue.http.headers.common['X-CSRF-TOKEN'] = $('meta[name="csrf-token"]').attr('content');
Vue.config.debug = false;

var router = new VueRouter({
    hashBang: false,
    history: true
});
var App = require('./app.vue');

var MainContent = require('./components/main/index.vue');

router.map({
    '*' : {
        component: require('./components/main/content/not-found.vue')
    },
    '/' : {
        name: 'dashboard',
        component: MainContent
    },
    '/settings' : {
        name: 'settings',
        component: MainContent
    }
});

router.start(App, 'body');

var $ = require('jquery');
var VueRouter = require('vue-router');

window.Vue = require('vue');
Vue.use(VueRouter);
Vue.use(require('vue-resource'));
Vue.http.options.root = '/api';
Vue.http.headers.common['X-CSRF-TOKEN'] = $('meta[name="csrf-token"]').attr('content');
Vue.config.debug = true;

var router = new VueRouter({
    hashBang: false,
    history: true,
    linkActiveClass: 'active'
});

var App = require('./app.vue');
var MainContent = require('./components/main/index.vue');

var Dashboard = require('./components/main/content/dashboard.vue');
var Outputs = require('./components/main/content/outputs.vue');
var Overlays = require('./components/main/content/overlays.vue');
var Playlists = require('./components/main/content/playlists.vue');
var Schedule = require('./components/main/content/schedule.vue');
var Files = require('./components/main/content/files.vue');
var Plugins = require('./components/main/content/plugins.vue');
var Events = require('./components/main/content/events.vue');
var Effects = require('./components/main/content/effects.vue');
var Testing = require('./components/main/content/testing.vue');

var SettingsContent = require('./components/main/settings.vue');
var GeneralSettings = require('./components/main/content/settings/general.vue');
var DateSettings = require('./components/main/content/settings/date.vue');
var EmailSettings = require('./components/main/content/settings/email.vue');
var NetworkSettings = require('./components/main/content/settings/network.vue');
var LogSettings = require('./components/main/content/settings/logs.vue');
var AdvancedSettings = require('./components/main/content/settings/advanced.vue');

router.map({
    '*' : {
        component: require('./components/main/content/not-found.vue')
    },
    '/' : {
        name: 'dashboard',
        component: MainContent,
        subRoutes: {
            '/' : {
                name: 'dashboard',
                component: Dashboard,

            },
            '/outputs' : {
                name: 'outputs',
                component: Outputs
            },
            '/overlays' : {
                name: 'overlays',
                component: Overlays
            },
            '/files' : {
                name: 'files',
                headerName: 'File Manager',
                component: Files
            },
            '/playlists' : {
                name: 'playlists',
                component: Playlists
            },
            '/schedule' : {
                name: 'schedule',
                component: Schedule
            },
            '/plugins' : {
                name: 'plugins',
                component: Plugins
            },
            '/events' : {
                name: 'events',
                component: Events
            },
            '/effects' : {
                name: 'effects',
                component: Effects
            },
            '/testing' : {
                name: 'testing',
                component: Testing
            },

        }
    },
   
    '/settings' : {
        name: 'settings',
        component: SettingsContent,
        subRoutes: {
            '/' : {
                component: GeneralSettings
            },
            '/date' : {
                component: DateSettings
            },
             '/email' : {
                component: EmailSettings
            },
             '/network' : {
                component: NetworkSettings
            },
             '/logs' : {
                component: LogSettings
            },
            '/advanced' : {
                component: AdvancedSettings
            },

        }
    }
});

router.start(App, 'body');

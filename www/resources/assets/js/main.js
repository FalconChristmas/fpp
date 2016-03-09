var $ = require('jquery');
var VueRouter = require('vue-router');
var VueValidator = require('vue-validator');
window.Vue = require('vue');
Vue.use(VueValidator);
Vue.use(VueRouter);
Vue.use(require('vue-animated-list'));
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
var MainContent = require('./components/index.vue');

var Dashboard = require('./components/dashboard.vue');
var Outputs = require('./components/outputs/index.vue');
var Overlays = require('./components/overlays.vue');
var Playlists = require('./components/playlists.vue');
var Schedule = require('./components/schedule.vue');
var Files = require('./components/files.vue');
var Plugins = require('./components/plugins.vue');
var Events = require('./components/events.vue');
var Effects = require('./components/effects.vue');
var Testing = require('./components/testing.vue');

var E131Output = require('./components/outputs/e131.vue');
var DMXOutput = require('./components/outputs/dmx.vue');
var PanelOutput = require('./components/outputs/panels.vue');
var OtherOutput = require('./components/outputs/other.vue');

var SettingsContent = require('./components/settings.vue');
var GeneralSettings = require('./components/settings/general.vue');
var DateSettings = require('./components/settings/date.vue');
var EmailSettings = require('./components/settings/email.vue');
var NetworkSettings = require('./components/settings/network.vue');
var LogSettings = require('./components/settings/logs.vue');
var AdvancedSettings = require('./components/settings/advanced.vue');

router.map({
    '*' : {
        component: require('./components/not-found.vue')
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
                headerName: 'Channel Outputs',
                component: Outputs,
                headerMenu: true,
                menuName: 'Outputs',
                menu: [
                    { path: '/outputs', label: 'E131' },
                    { path: '/outputs/dmx', label: 'Pixelnet / DMX' },
                    { path: '/outputs/panels', label: 'LED Panels' },
                    { path: '/outputs/other', label: 'Other' },
                ],
                subRoutes: {
                    '/' : {
                        component: E131Output
                    },
                    '/dmx' : {
                        component: DMXOutput
                    },
                    '/panels' : {
                        component: PanelOutput
                    },
                    '/other' : {
                        component: OtherOutput
                    }

                }
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

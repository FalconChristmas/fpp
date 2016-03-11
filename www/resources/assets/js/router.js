import VueRouter from "vue-router"; 

Vue.use(VueRouter);

import MainContent from './components/index.vue';
import NotFound from './components/not-found.vue';
import Dashboard from './components/dashboard.vue';
import Outputs from './components/outputs/index.vue';
import Overlays from './components/overlays.vue';
import Playlists from './components/playlists.vue';
import Schedule from './components/schedule.vue';
import Files from './components/files.vue';
import Plugins from './components/plugins.vue';
import Events from './components/events.vue';
import Effects from './components/effects.vue';

import Testing from './components/testing/index.vue';
import ChannelTesting from "./components/testing/channels.vue";
import SequenceTesting from "./components/testing/sequence.vue";

import E131Output from './components/outputs/e131.vue';
import DMXOutput from './components/outputs/dmx.vue';
import PanelOutput from './components/outputs/panels.vue';
import OtherOutput from './components/outputs/other.vue';

import SettingsContent from './components/settings.vue';
import GeneralSettings from './components/settings/general.vue';
import DateSettings from './components/settings/date.vue';
import EmailSettings from './components/settings/email.vue';
import NetworkSettings from './components/settings/network.vue';
import LogSettings from './components/settings/logs.vue';
import AdvancedSettings from './components/settings/advanced.vue';

const router = new VueRouter({
    hashBang: false,
    history: true,
    linkActiveClass: 'active'
});


router.map({
    '*' : {
        component: NotFound
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
                component: Testing,
                headerName: 'Display Testing',
                headerMenu: true,
                menuName: 'Testing',
                menu: [
                    { path: '/testing', label: 'Channel' },
                    { path: '/testing/sequence', label: 'Sequence' },
                ],
                subRoutes: {
                    '/' : {
                        component: ChannelTesting
                    },
                    '/sequence' : {
                        component: SequenceTesting
                    },
                }
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

module.exports = router;
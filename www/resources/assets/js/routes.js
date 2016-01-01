import MainContent from './components/main/index.vue';

/**
 * Top Level
 */
import Dashboard from './components/main/content/dashboard.vue';
import Outputs from './components/main/content/outputs.vue';
import Overlays from './components/main/content/overlays.vue';
import Playlists from './components/main/content/playlists.vue';
import Schedule from './components/main/content/schedule.vue';
import Files from './components/main/content/files.vue';
import Plugins from './components/main/content/plugins.vue';
import Events from './components/main/content/events.vue';
import Effects from './components/main/content/effects.vue';
import Testing from './components/main/content/testing.vue';

/**
 * Settings
 */
import SettingsContent from './components/main/settings.vue';
import GeneralSettings from './components/main/content/settings/general.vue';
import DateSettings from './components/main/content/settings/date.vue';
import EmailSettings from './components/main/content/settings/email.vue';
import NetworkSettings from './components/main/content/settings/network.vue';
import LogSettings from './components/main/content/settings/logs.vue';

export default {
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

        }
    }
};
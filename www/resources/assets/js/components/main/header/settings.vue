<template>
    <div class="header settings-header">
        <div class="page-meta">
            <h1 class="page-title">{{ $route.name | capitalize }}</h1>
        </div>
        <control-buttons></control-buttons>
        <ul class="settings-menu nav navbar-nav">
            <li class="nav-item"><a v-link="{path: '/settings', exact: true, activeClass: 'active'}">General</a></li>
            <li class="nav-item"><a v-link="{path: '/settings/date', activeClass: 'active'}">Date & Time</a></li>
            <li class="nav-item"><a v-link="{path: '/settings/email', activeClass: 'active'}">Email</a></li>
            <li class="nav-item"><a v-link="{path: '/settings/network', activeClass: 'active'}">Network</a></li>
            <li class="nav-item"><a v-link="{path: '/settings/logs', activeClass: 'active'}">Logs</a></li>
        </ul>
    </div>
</template>

<script>
import controlButtons from "../../shared/control-buttons.vue";

export default {
    components: { controlButtons },
    props: [],
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";
    
    .settings-header {
        
        flex-flow: wrap;
        padding: 20px 20px 0;

       
        .settings-menu {
            width: 100%;
            padding-top: 10px;
            
           
            a {
                display: block;
                text-transform: uppercase;
                letter-spacing: $letter-spacing;
                font-size: 11px;
                border-bottom: 3px solid transparent;
                padding: 3px 5px;
                margin-bottom: -1px;
                &.active {
                    font-weight: 600;
                    border-color: $alert-color;
                }
            }
        }
    }
</style>
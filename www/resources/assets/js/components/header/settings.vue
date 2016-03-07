<template>
    <div class="header settings-header">
        <div class="page-meta">
            <h1 class="page-title">{{ $route.name | capitalize }}</h1>
        </div>
        <control-buttons></control-buttons>
        <nav class="navbar settings-menu">
            <button class="navbar-toggler hidden-sm-up" type="button" data-toggle="collapse" data-target="#header-navbar">&#9776; Settings Menu</button>
            <div class="collapse navbar-toggleable-xs" id="header-navbar">
                <ul class="nav navbar-nav">
                    <li class="nav-item"><a v-link="{path: '/settings', exact: true, activeClass: 'active'}">General</a></li>
                    <li class="nav-item"><a v-link="{path: '/settings/date', activeClass: 'active'}">Date & Time</a></li>
                    <li class="nav-item"><a v-link="{path: '/settings/email', activeClass: 'active'}">Email</a></li>
                    <li class="nav-item"><a v-link="{path: '/settings/network', activeClass: 'active'}">Network</a></li>
                    <li class="nav-item"><a v-link="{path: '/settings/logs', activeClass: 'active'}">Logs</a></li>
                    <li class="nav-item"><a v-link="{path: '/settings/advanced', activeClass: 'active'}">Advanced</a></li>
                </ul>
            </div>
    </div>
</template>

<script>
import controlButtons from "../shared/control-buttons.vue";

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
        flex-direction: column;
       

       
        .settings-menu {
            width: 100%;
            padding: 10px 0 0;
           
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
       
        @include breakpoint($iphone6-ls-min) {
            flex-direction: row;
            
        }
    }
</style>
<template>
    <ul class="header-menu nav navbar-nav" :class="menuClass">
        <li class="nav-item" v-for="item in items">
            <a v-link="{ path: item.path, activeClass: 'active', exact: true}">{{item.label}}</a>
        </li>
    </ul>
</template>

<script>
export default {
    
    props: ['menuClass', 'items'],
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
    
  .header-menu {
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

</style>
<template>
    <div class="header" :class="{ 'settings-header' : $route.headerMenu  }">
        <div class="page-meta">
            <h1 class="page-title">{{ $route.headerName ? $route.headerName : $route.name | capitalize }}</h1>
        </div>
        <control-buttons></control-buttons>
        <nav v-if="$route.headerMenu" class="navbar header-menu">
            <button class="navbar-toggler hidden-sm-up" type="button" data-toggle="collapse" data-target="#header-navbar">&#9776; {{$route.menuName}} Menu</button>
            <div class="collapse navbar-toggleable-xs" id="header-navbar">
                <header-menu v-if="$route.headerMenu" :items="$route.menu"></header-menu>
            </div>
        </nav>
    </div>
</template>

<script>
import controlButtons from "../shared/control-buttons.vue";
import headerMenu from "../shared/header-menu.vue";

export default {
    components: { controlButtons, headerMenu },
    props: ['hasHeaderMenu'],
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {}

}
</script>
<style>
.navbar.header-menu {
    padding: 10px 0 0;
}
</style>
<template>
    <div class="header">
        <div class="page-meta">
            <h1 class="page-title">{{ $route.name | capitalize }}</h1>
        </div>
        <control-buttons></control-buttons>
        <header-menu v-if="$route.headerMenu"></header-menu>
    </div>
</template>

<script>
import controlButtons from "../../shared/control-buttons.vue";
import headerMenu from "../../shared/header-menu.vue";

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

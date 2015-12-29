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

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";
    
    .header {
        background: white;
        display: flex;
        flex-direction: row;
        justify-content: space-between;
        align-items: center;
        width: 100%;
        padding: 20px;

        .page-title {
            margin: 0;
            font-weight: 300;
        }
        .button {
            margin-bottom: 0;
        }
    }
</style>
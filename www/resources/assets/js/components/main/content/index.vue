<template>
    <div class="content">
        <dashboard v-show="$route.name == 'dashboard'"></dashboard>
        <settings v-show="$route.name == 'settings'"></settings>
    </div>
</template>

<script>
import Dashboard from "./dashboard.vue";
import Settings from "./settings.vue";

export default {
    components: { Dashboard, Settings },
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
    
    .content {
        padding: 20px;
    }
</style>
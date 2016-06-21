<template>
    <div class="outputs">
        <router-view></router-view>
    </div>
</template>

<script>
import { getAllOutputs } from "../../state/actions";

export default {
    components: {},
    props: [],
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {},
    created() {
        this.getAllOutputs();
    },
    vuex: {

        actions: {
            getAllOutputs
        }
    }

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

</style>

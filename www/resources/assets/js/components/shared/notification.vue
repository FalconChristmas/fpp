<template>
    <div class="notification-bar">
        <div class="notification-content">
            <button type="button" v-if="close" class="close"><span aria-hidden="true">×</span><span class="sr-only">Close</span></button>
            <span v-html="text" v-if="text"></span>
            <slot name="content" v-if="!text"></slot>
        </div>
    </div>
</template>

<script>
export default {
    components: {},
    props: {
        close: {
            type: Boolean,
            default: true
        },
        text: {
            type: String,
            default: '',
        },
        position: {
            type: String,
            default: 'top center'
        },
        type: {
            type: String,
            default: 'bar'
        }
    },
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {},
    vuex: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

</style>
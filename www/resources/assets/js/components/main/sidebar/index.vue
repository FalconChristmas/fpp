<template>
    <div class="content-sidebar">
        <ul class="nav">
            <li v-for="item in items">
                <a v-link="item.link">{{item.label}}</a>
            </li>
        </ul>
    </div>
</template>

<script>
export default {
    components: {},
    props: ['items'],
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
    
    .content-sidebar {
        width: 155px;
       // position: absolute;
        padding: 20px;
        border-right: 1px solid #e7e7e7;
        transition: width 0.35s;
    }
</style>
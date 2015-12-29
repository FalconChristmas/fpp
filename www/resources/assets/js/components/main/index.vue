<template>
    <div id="main-wrapper">
        <main-header></main-header>
        <main-content></main-content>
        <main-footer></main-footer>
    </div>
</template>

<script>
    import mainHeader from './header/index.vue';
    import mainContent from './content/index.vue';
    import mainFooter from "./footer/index.vue";

    export default {
        components: { mainHeader, mainContent, mainFooter },
    };
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

    #main-wrapper {
        display: flex;
        flex: 1;
        flex-direction: column;
    }
</style>

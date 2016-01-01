<template>
    <div id="main-wrapper">
        <settings-header></settings-header>
        <settings-content></settings-content>
        <main-footer></main-footer>
    </div>
</template>

<script>
    import settingsHeader from './header/settings.vue';
    import settingsContent from './content/settings.vue';
    import mainFooter from "./footer/index.vue";

    export default {
        components: { settingsHeader, settingsContent, mainFooter },
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

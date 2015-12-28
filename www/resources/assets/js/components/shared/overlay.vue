<template>
    <div id="overlay">
        <img class="logo" src="/img/logo.svg">
        <loading></loading>
    </div>
</template>

<script>
    import loading from "./falcon-loading.vue";
    export default {
        components: { loading }
    };
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

    #overlay {
        position: fixed;
        top: 0;
        left: 0;
        z-index: 9999;
        width: 100%;
        height: 100%;
        background-color: $dark-blue;

        @include vertical-center();
        flex-direction: column;

        .logo {
            width: 220px;
            height: 108px;
            margin-bottom: 15px;
            position: relative;
            left: -25px;
        }
       
    }

</style>

<template>
    <div class="control-buttons">
        <a href="#" class="button small restart-fppd">Restart FPPD</a>
        <a href="#" class="button small reboot-pi">Reboot Pi</a>
        <a href="#" class="button small shutdown-pi">Shutdown Pi</a>
    </div>
</template>

<script>
export default {
   
    methods: {
        restartFppd() {},
        rebootPi() {},
        shutdownPi() {}
    }
   

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";
    
    .control-buttons {
        margin-bottom: 1rem;
        .button {
            @include gradient-button(#f8f8f8, #eaeaea);
        }

        @include breakpoint($iphone6-ls-min) {
            margin: 0;
        }
    }
</style>
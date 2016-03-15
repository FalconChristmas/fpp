<template>
     <div class="alert-bar" :class="classes">
        <div class="alert-content custom-alert" v-if="message">
            <span v-html="message" v-if="message" class="alert-message"></span>
            <button v-if="button" class="button small gradient" v-html="buttonTxt" @click="buttonClick"></button>
        </div>
        <div class="alert-content reboot bg-danger" v-if="reboot">
            <span class="alert-message">Settings change requires a reboot</span>
            <button class="button small gradient" @click="buttonClick">Reboot Now</button>
        </div>
        <div class="alert-content restart bg-danger" v-if="restart">
            <span class="alert-message">Settings change requires FPPD to be restarted</span>
            <button class="button small gradient" @click="buttonClick">Restart FPPD</button>
        </div>
    </div>
</template>

<script>
export default {
    props: {
        message: {
            type: String,
            default: ''
        },
        buttonTxt: {
            type: String,
            default: ''
        },
        button: {
            type: Boolean,
            default: false
        },
        buttonClick: {
            type: Function,
            default: function() {}
        },
        classes: {
            type: Array,
            default: function() {
                return [];
            }
        }
    },
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {},
    vuex: {

        getters: {
            reboot: state => state.shared.rebootFlag,
            restart: state => state.shared.restartFlag,
            update: state => state.shared.updateFlag,
        }
    }

}
</script>

<style lang="sass">
@import "resources/assets/sass/_vars.scss";
@import "resources/assets/sass/_mixins.scss";
.alert-bar {
    display: block;
    overflow: hidden;

    .alert-content {
        padding: 10px;
        background: $primary-color;
        text-align: center;
        margin-bottom: 1px;
        .button {
            margin: 0 0 0 10px;
        }
    }
    .alert-message {
        text-transform: uppercase;
        letter-spacing: 1px;
        color: white;
        vertical-align: middle;
    }
}
</style>
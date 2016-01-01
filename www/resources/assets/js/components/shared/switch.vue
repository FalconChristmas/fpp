<template>
    <label class="label-switch">
      <input type="checkbox" v-model="model" />
      <div class="checkbox"></div>
    </label>
</template>

<script>
export default {
    components: {},
    props: {
        model: {
            required: true,
            twoWay: true
        }
    },
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {}

}
</script>

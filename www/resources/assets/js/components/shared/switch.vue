<template>
    <label class="label-switch" :class="{ 'small' : size == 'small' }">
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
        },
        size: {
            type: String,
            default: 'normal'
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

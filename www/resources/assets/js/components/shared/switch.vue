<template>
    <label class="label-switch" :class="{ 'small' : size == 'small' }">
      <input type="checkbox" v-model="model" @change="handleChange(model)" />
      <div class="checkbox"></div>
    </label>
</template>

<script>
export default {
    props: {
        model: {
            required: true,
        },
        size: {
            type: String,
            default: 'normal'
        },
        checked: {
            type: Boolean,
            default: false
        }
    },
    methods: {
        handleChange(model) {
            this.$dispatch('toggle', [model]);
        }
    }
    
}
</script>

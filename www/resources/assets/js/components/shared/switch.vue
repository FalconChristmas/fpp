<template>
    <label class="label-switch" :class="{ 'small' : size == 'small' }">
      <input type="checkbox" v-model="model" @change="onChange(model)" />
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
        onChange: {
            type: Function,
            default: function() {}
        }
    }
}
</script>

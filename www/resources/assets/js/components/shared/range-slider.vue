<template>
<div class="v-range-slider">
  <slot name="left"></slot>
  <input type="range" v-model="value" :min="min" :max="max" :step="step" :name="name">
  <slot name="right"></slot>
</div>
</template>

<script>
export default {
  props: {
    min: {
      type: Number,
    },
    max: {
      type: Number,
    },
    step: {
      type: Number,
    },
    name: {
      type: String,
    },
    value: {
      type: Number,
    }
  }
}
</script>

<style lang="sass">
@import "resources/assets/sass/_vars.scss";
.v-range-slider {
  display: flex;
  align-items: center;
  padding: 2px 11px;
}
.v-range-slider input[type=range] {
  -webkit-appearance: none;
  flex: 1;
  display: block;
  overflow: hidden;
  margin: 5px 10px;
  padding: 16px 1px 20px;
  box-sizing: border-box;
  outline: none;
  -webkit-tap-highlight-color: transparent;
}
.v-range-slider input::-webkit-slider-runnable-track {
  position: relative;
  height: 3px;
  background: #ddd;
  border: none;
  border-radius: 3px;
}
.v-range-slider input::-webkit-slider-thumb {
  -webkit-appearance: none;
  position: relative;
  margin-top: -9px;
  cursor: pointer;
  width: 20px;
  height: 20px;
  border: 0;
  border-radius: 50%;
  background: white;
  box-shadow: 0 0 2px rgba(0, 0, 0, 0.4);
    &:before {
      position: absolute;
      display: inline-block;
      content: ' ';
      top: 9px;
      left: -2001px;
      width: 2000px;
      height: 3px;
      background: $color-success;
  }
}

</style>
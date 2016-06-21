<template>
<div class="picker-group">
  <div class="picker-label" v-if="label">{{label}}</div>
  <div class="picker-swatch" @click="handleClick">
    <div class="picker-color" :style="{background: this.colors.hex}"></div>
  </div>
  <div class="picker-popover" v-if="displayPicker">
    <div class="picker-cover" @click="handleClose"></div>
    <sketch :colors.sync="colors"></sketch>
  </div>
</div>
</template>

<script>
import { Sketch } from 'vue-color';

const defaultColors = {
  hex: '#194d33',
  hsl: {
    h: 150,
    s: 0.5,
    l: 0.2,
    a: 1
  },
  hsv: {
    h: 150,
    s: 0.66,
    v: 0.30,
    a: 1
  },
  rgba: {
    r: 25,
    g: 77,
    b: 51,
    a: 1
  },
  a: 1
}

export default {
  components: {
    Sketch
  },
  props: {
    colors: {
      type: Object,
      twoWay: true
    },
    label: {
      type: String
    }
  },
  data() {
    return {
      displayPicker: false,
    }
  },
  methods: {
    handleClose() {
      this.displayPicker = false;
    },

    handleChange() {

    },

    handleClick() {
      this.displayPicker = !this.displayPicker;
    }

  }

}
</script>

<style lang="sass">
@import "resources/assets/sass/_vars.scss";
@import "resources/assets/sass/_mixins.scss";
.picker-group {
  // display: flex;
  // flex-direction: row;
  // flex-wrap: nowrap;
  // margin-bottom: 10px;
  margin: 5px 0;

}
.picker-label {
  margin-right: 10px;
}
.picker-color {
    width: 36px;
    height: 14px;
    border-radius: 2px;
    //background: `rgba(${ this.state.color.r }, ${ this.state.color.g }, ${ this.state.color.b }, ${ this.state.color.a })`,
  }
.picker-swatch {
  padding: 5px;
  background: #fff;
  border-radius: 1px;
  box-shadow: 0 0 0 1px rgba(0,0,0,.1);
  display: inline-block;
  cursor: pointer;
}
.picker-popover {
    position: absolute;
    z-index: 2;
}
.picker-cover {
  position: fixed;
  top: 0;
  right: 0;
  bottom: 0;
  left: 0;
}
</style>

<template>
<div id="channel-testing-settings" class="page-container box">
     <div class="box-header">
         <h4 class="box-title"><span class="semi-bold">Channel Output</span> Testing</h4>
     </div>
     <div class="box-tool">
         <div class="enable-output">
           <small>Enable Test Mode </small>
           <switch :model="enabled" @toggle="toggleMode"></switch>
         </div>
     </div>
     <div class="box-actions">
       <div class="channel-testing-range">
            <div class="range-inputs level">
              <div class="level-left">
                <div class="level-item">
                   <form-input
                    :model.sync="start_channel"
                    type="number"
                    label="Start Channel"
                    size="sm"
                    state-icon>
                  </form-input>
                </div>

                <div class="level-item">
                   <form-input
                    :model.sync="end_channel"
                    type="number"
                    label="End Channel"
                    size="sm"
                    state-icon>
                  </form-input>
                </div>

                <div class="level-item">
                  <div class="form-group">
                    <label class="control-label p-l-20">Update Interval</label>
                    <range-slider :min="100" :max="5000" :step="100" :value.sync="update_interval"><span slot="right">{{update_interval}}ms</span></range-slider>
                  </div>
                </div>

              </div>
              <div class="level-right">
                <div class="level-item has-text-centered">
                  <button class="button gradient small bold m-a-0">All Lights On</button>
                </div>
                <div class="level-item has-text-centered">
                   <button class="button gradient small bold m-a-0">All Lights Off</button>
                </div>
              </div>

            </div>
          </div>
     </div>
     <div class="box-body">
        <form>
          <div class="test-patterns">
            <h4 class="test-patterns-title semi-bold">Solid Color Patterns</h4>
            <div class="test-patterns-group test-patterns-single">
              <div class="test-patterns-config">
                <div class="form-group">
                  <label class="control-label p-l-20 m-b-0">Brightness</label>
                  <range-slider :min="1" :max="255" :step="1" :value.sync="solid_brightness"><span slot="right">{{solid_brightness}}</span></range-slider>
                </div>
                 <div class="form-group m-b-0" v-if="activePattern == 'solidchase'">
                  <label class="control-label p-l-20 m-b-0">Chase Size</label>
                  <range-slider :min="2" :max="6" :step="1" :value.sync="solid_chase_size"><span slot="right">{{solid_chase_size}}</span></range-slider>
                </div>
              </div>
              <div v-for="pattern in solidpatterns" :class="['test-pattern', 'text-xs-center', { 'active' : this.activePattern == pattern.name }]" @click="activatePattern(pattern.name)">
                  <div class="test-pattern-type">{{pattern.color}}</div>
                  <div class="test-pattern-pattern">{{pattern.pattern}}</div>
                  <div class="test-pattern-type">{{pattern.type}}</div>
              </div>
            </div>

             <h4 class="test-patterns-title semi-bold">RGB Patterns</h4>
            <div class="test-patterns-group test-patterns-rgb">
              <div class="test-patterns-config">
                <div class="pattern-colors" v-if="activePattern != 'rgbfill'">
                  <picker :colors.sync="r" label="Color A"></picker>
                  <picker :colors.sync="g" label="Color B"></picker>
                  <picker :colors.sync="b" label="Color C"></picker>
                </div>
                <picker :colors.sync="b" v-if="activePattern == 'rgbfill'" label="Fill Color"></picker>
              </div>
                <div v-for="pattern in rgbpatterns" :class="['test-pattern', 'text-xs-center', { 'active' : this.activePattern == pattern.name }]" @click="activatePattern(pattern.name)">
                    <div class="test-pattern-type">{{pattern.color}}</div>
                    <div class="test-pattern-pattern">{{pattern.pattern}}</div>
                    <div class="test-pattern-type">{{pattern.type}}</div>
                </div>

            </div>
          </div>
        </form>
     </div>
</div>
</template>

<script>
import Switch from "../shared/switch.vue";
import FormInput from "../shared/input.vue";
import RangeSlider from "../shared/range-slider.vue";
import Picker from "../shared/picker.vue";

//import { Sketch } from "vue-color";

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
const rgbpatterns = [
  { name: 'abc', color: 'RGB', type: 'Chase', pattern: 'A • B • C' },
  { name: 'abca', color: 'RGB', type: 'Chase', pattern: 'A • B • C • All' },
  { name: 'abcn', color: 'RGB', type: 'Chase', pattern: 'A • B • C • None' },
  { name: 'abcan', color: 'RGB', type: 'Chase', pattern: 'A • B • C • All • None' },
  { name: 'rgbfill', color: 'RGB', type: 'Fill', pattern: 'Fill Color' },
];

const solidpatterns = [
   { name: 'solidfill', color: 'Solid', type: 'Fill', pattern: 'Fill Color' },
  { name: 'solidchase', color: 'Solid', type: 'Chase', pattern: 'Solid Chase' },
];

export default {
    components: { Switch, FormInput, RangeSlider, Picker },
    props: [],
    data() {
        return {
          colors: defaultColors,
          start_channel: 1,
          end_channel: 524288,
          update_interval: 1000,
          activePattern: null,
          rgbpatterns: rgbpatterns,
          solidpatterns: solidpatterns,
          solid_brightness: 200,
          solid_chase_size: 2,
          r: { hex: '#FF0000', a: 1 },
          g: { hex: '#00FF00', a: 1 },
          b: { hex: '#0000FF', a: 1 }
        };
    },
    ready() {},
    methods: {
        toggleMode(val) {
            console.log('Received Val:' + val);
        },
        activatePattern(pattern) {
          this.activePattern = pattern;
        }
    },
    events: {},
    vuex: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

  .channel-testing-range {
    padding: 0 10px;
  }
  .pattern-colors {
    display: flex;
    flex-flow: row wrap;
    justify-content: space-between;
    align-items: center;

  }
  .test-patterns-group {
    display: flex;
    flex-direction: row;
    flex-wrap: wrap;
    align-items: center;
    padding: 10px 0 25px;
  }

  .test-patterns-config {
    flex-basis: 20%; //flex: 0 0 20%;
    padding: 10px;
    background: #f8f8f8;
     border: 1px solid #e8e8e8;
    min-height: 125px;
    display: flex;
    flex-direction: column;
    justify-content: center;
    margin-right: 5px;
  }
  .test-pattern {
    cursor: pointer;
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
    width: 135px;
    height: 125px;
    margin: 5px;
    border: 1px solid #e5e5e5;
    transition: all 0.35s;

    &.active {
      background-color: $color-azure;
      border-color: $color-blue;
      color: white;

      .test-pattern-color {
         &:first-child,
         &:last-child {
          border-color: $color-blue;
        }
      }
    }

    &-colors {
      display: flex;
      flex-direction: row;
      flex-wrap:no-wrap;
      align-content: center;
      justify-content: stretch;
      background: #f5f5f5;
      padding: 0;
      border-radius: 0 !important;
    }

    &-type {
      text-transform: uppercase;
      letter-spacing: 3px;
      padding: 5px;
      font-size: .8em;
      opacity: .5;
    }
    &-pattern {
      //font-size: 1.5em;
      padding: 5px;
    }

    &-color {
      flex: 1;
      padding: 10px 0;
      &:first-child {
        border-right: 1px solid #e5e5e5;
      }
      &:last-child {
        border-left: 1px solid #e5e5e5;
      }
    }
  }
</style>

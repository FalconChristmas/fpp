<template>
     <div id="e131-settings" class="outputs-container box">
     <div class="box-header">
         <h4 class="box-title"><span class="semi-bold">E131</span> Outputs</h4>
     </div>
     <div class="box-tool">
         <div class="enable-output">
           <small>Enable Outputs </small>
           <switch :model="enabled" :on-change="onToggle"></switch>
         </div>
     </div>
      <form>
      <div class="box-actions level">
        <div class="level-left">

        </div>
        <div class="level-right">
          <div class="level-item">
            <form-input
                :model.sync="search"
                type="text"
                label="Search"
                size="sm"
                state-icon>
              </form-input>
          </div>
          <div class="level-item">
            <button class="button gradient submit bold m-a-0" @click.prevent="$broadcast('show::modal', 'output-modal')">Add Output</button>
          </div>
        </div>
      </div>
       <div class="output-actions">
                <div class="filter-row align-end">
                    <div class="filter text-xs-center">
                        <span>Active</span>
                        <switch :model.sync="defActive" size="small"></switch>
                    </div>
                     <div class="filter text-xs-center">
                        <span>Increment Universe</span>
                        <switch :model.sync="incrementUniv" size="small"></switch>
                    </div>
                     <div class="filter text-xs-center">
                        <span>Increment Start</span>
                        <switch :model.sync="incrementStart" size="small"></switch>
                    </div>
                </div>
                <div class="filter-row">
                    <div class="filter">
                         <form-input
                          :model.sync="defStart"
                          type="number"
                          label="Start"
                          size="sm"
                          state-icon>
                        </form-input>
                    </div>
                    <div class="filter">
                         <form-input
                          :model.sync="defSize"
                          type="number"
                          label="Size"
                          size="sm"
                          state-icon>
                        </form-input>
                    </div>

                    <div class="filter">
                        <div class="form-group">
                            <label for="" class="control-label">Type</label>
                            <div class="input">
                                <select v-model="defType" class="c-select" style="padding-top: 0.25rem; padding-bottom: 0.275rem;">
                                    <option value="unicast">Unicast</option>
                                    <option value="multicast">Multicast</option>
                                </select>
                            </div>
                        </div>
                    </div>
                    <div class="filter">
                        <form-input
                          :model.sync="defAddress"
                          type="text"
                          label="Address"
                          size="sm"
                          state-icon>
                        </form-input>

                    </div>
                    <div class="filter">
                         <form-input
                          :model.sync="quantity"
                          type="number"
                          label="# of Outputs"
                          size="sm"
                          state-icon>
                        </form-input>
                    </div>
                </div>
                <div class="filter-row">

                    <button @click="addOutput" type="button" class="button gradient submit pull-xs-right m-a-0">{{ addOutputLabel }}</button>
                </div>

            </div>
     <div class="box-body">


            <!-- <div id="universe-outputs"> -->
                <div class="universe-list output-list table-responsive">
                    <!-- <div class="table-responsive"> -->
                        <table class="table table-sm table-hover">
                            <thead>
                            <tr>
                                <th class="text-xs-center universe-active"></th>
                                <th class="text-xs-center universe-index">#</th>
                                <th class="text-xs-center universe-number">Universe</th>
                                <th class="text-xs-center universe-start">Start</th>
                                <th class="text-xs-center universe-size">Size</th>
                                <th class="text-xs-center universe-type">Type</th>
                                <th class="text-xs-center universe-address" style="width: 150px">Unicast Address</th>
                                <th class="text-xs-center universe-label" >Label</th>
                                <th class="text-xs-center universe-action p-r-1" style="width: 45px"></th>
                            </tr>
                            </thead>
                            <tbody>
                                <tr is="universe" v-for="(index, universe) in outputs" :index="index" :universe.sync="universe"></tr>
                            </tbody>
                        </table>
                    <!-- </div> -->

                </div>
            <!-- </div> -->

        </div>
        </form>
        <modal type="e131" :fields="getModalFields()"></modal>
    </div>
</template>

<script>
import Switch from "../shared/switch.vue";
import Universe from "./universe.vue";
import Dropdown from "../shared/dropdown-select.vue";
import Checkbox from "../shared/checkbox.vue";
import FormInput from "../shared/input.vue";
import outputMixin from "../../mixins/outputMixin";
import Modal from "./output-modal.vue";

import {
    toggleOutputs
} from "../../state/actions";

const universeStub =  {
        active: false,
        number: 1,
        start: 1,
        type: 'unicast',
        size: 512,
        address: null,
        label: null
    };


export default {
    components: { Universe, Switch, Dropdown, Checkbox, FormInput, Modal  },
    mixins: [outputMixin],
    props: [],
    data() {
        return {
            defActive: true,
            defType: 'unicast',
            defSize: 512,
            defStart: 1,
            defAddress: null,
            incrementStart: true,
            incrementUniv: true,
            quantity: 1,

        };
    },
    ready() {},
    methods: {
        onToggle(model) {
            this.toggleOutputs('e131', model);
        },
        assembleNewOutput(last = {universe: 0, start: 0, size: 0}) {
            return {
                active: true,
                universe: this.incrementUniv ? last.universe + 1 : 1,
                start: this.incrementStart ? (last.start + last.size) + 1 : 1,
                type: this.defType,
                size: this.defSize,
                address: this.defAddress,
                label: null
            };
        },
        getModalFields() {
          return [
            { type: 'number', name: 'Start Channel', model: this.defStart },
            { type: 'number', name: 'Size', model: this.defSize },
            { type: 'select', name: 'Type', model: this.defType, options: [ 'unicast', 'multicast' ] },
            { type: 'text', name: 'Address', model: this.defAddress },
            { type: 'number', name: '# of Outputs', model: this.quantity },
          ];


        }
    },
    vuex: {
        actions: {
            toggleOutputs
        },
        getters: {
            outputs: state => state.outputs.e131.outputs,
            enabled: state => state.outputs.e131.enabled
        }
    }


}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

    .universe-list {

        .form-control {
            padding: .25rem .50rem;
            line-height: 1.3;
        }
        .c-select {
            padding: .22rem 1.5rem .23rem .50rem;
        }

        .universe {
            &-index {
                min-width: 50px;
                width: 50px;
                text-align: center;
            }
            &-active {
                min-width: 50px;
                width: 50px;
                text-align: center;
            }
            &-number {
                min-width: 80px;
                width: 80px;
                text-align: center;
            }
            &-start {
                min-width: 80px;
                width: 80px;
                text-align: center;
            }
            &-size {
                min-width: 80px;
                width: 80px;
                text-align: center;
            }
            &-type {
                min-width: 115px;
                width: 115px;
                text-align: center;
            }
            &-address {
                min-width: 150px;
                width: 150px;
                text-align: center;
            }
            &-label {


            }
            &-action {
                text-align: center;
            }
        }

    }

    .enable-output {
        small, .label-switch {
            display: inline-block;
            margin-bottom: 0;
            vertical-align: middle;
        }
    }
</style>

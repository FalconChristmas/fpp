<template>
     <div id="e131-settings" class="outputs-container container-fluid box">   
     <div class="box-header">
         <h4 class="box-title"><span class="semi-bold">E131</span> Outputs</h4>     
     </div>
     <div class="box-tool">
         <div class="enable-output">
           <small>Enable Outputs </small>
           <switch :model.sync="enable_e131"></switch>
         </div>
     </div>
     <div class="box-body">
        <form>
            <div class="output-actions">
                <div class="filter-row">
                    <div class="filter text-xs-center">
                        <p>Active</p>
                        <switch :model.sync="defActive" size="small"></switch>
                    </div>
                     <div class="filter text-xs-center">
                        <p>Inc. Universe</p>
                        <switch :model.sync="incrementUniv" size="small"></switch>
                    </div>
                     <div class="filter text-xs-center">
                        <p>Inc. Start</p>
                        <switch :model.sync="incrementStart" size="small"></switch>
                    </div>
                </div>
                <div class="filter-row">
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
                    <button @click="addOutput" type="button" class="button btn-primary pull-xs-right m-a-0">Add Output</button>
                </div>
                    
            </div>
            <!-- <div id="universe-outputs"> -->
                <div class="universe-list table-responsive">
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
                                <tr is="universe" v-for="(index, universe) in universes" :index="index" :universe.sync="universe"></tr>
                            </tbody>
                        </table>
                    <!-- </div> -->
                    
                </div>
            <!-- </div> -->
        </form>
        </div>
    </div>
</template>

<script>
import Switch from "../../../shared/switch.vue";
import Universe from "./universe.vue";
import Dropdown from "../../../shared/dropdown-select.vue";
import Checkbox from "../../../shared/checkbox.vue";
import FormInput from "../../../shared/input.vue";

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
    components: { Universe, Switch, Dropdown, Checkbox, FormInput  },
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
            universes: [
                {active: true, number: 1, start: 1, type: 'unicast', size: 512, address: '10.10.10.10', label: 'The Label' },
                {active: true, number: 2, start: 513, type: 'unicast', size: 512, address: '10.10.10.10', label: 'The Label' }
            ],
            

        };
    },
    ready() {},
    methods: {
        addOutput() {
            for(let i = 0; i < this.quantity; i++) {
                let last = this.getLastOutput();
                let universe = {
                    active: true,
                    number: this.incrementUniv ? last.number + 1 : 1,
                    start: this.incrementStart ? (last.start + last.size) + 1 : 1,
                    type: this.defType,
                    size: this.defSize,
                    address: this.defAddress,
                    label: null
                };
        
                this.universes.push(universe);
            }
            
        },
        removeOutput(id) {
            if (id !== -1) {
              this.universes.splice(id, 1);
            }
        },
        getLastOutput() {
            if(this.universes.length) {
                return this.universes[this.universes.length-1];
            }
        }
    },
    events: {
        'remove-output' : function(id) {
            this.removeOutput(id);
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
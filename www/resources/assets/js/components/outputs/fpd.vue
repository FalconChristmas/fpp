<template>
    <div id="fpd-settings" class="outputs-container container-fluid box">
      <div class="box-header">
         <h4 class="box-title"><span class="semi-bold">FPD</span> Outputs</h4>
     </div>
     <div class="box-tool">
         <div class="enable-output">
           <small>Enable Outputs </small>
           <switch :model.sync="enable_pdmx"></switch>
         </div>
     </div>
      <div class="box-body">

        <form>
            <div class="output-actions">
                <div class="filter-row align-end">
                    <div class="filter text-xs-center">
                        <span>Active</span>
                        <switch :model.sync="defActive" size="small"></switch>
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
                          :model.sync="defCount"
                          type="number"
                          label="Channel Count"
                          size="sm"
                          state-icon>
                        </form-input>
                    </div>
                    <div class="filter">
                        <div class="form-group">
                            <label for="" class="control-label">Type</label>
                            <div class="input">
                                <select v-model="defType" class="c-select" style="padding-top: 0.25rem; padding-bottom: 0.275rem;">
                                    <option value="pixelnet">Pixelnet</option>
                                    <option value="dmx">DMX</option>
                                </select>
                            </div>
                        </div>
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
                    <div class="filter">
                        <button @click="addOutput" type="button" class="button btn-primary pull-xs-right m-a-0">{{ addOutputLabel }}</button>
                    </div>
                </div>

            </div>
             <div class="pdmx-list output-list table-responsive">
                    <table class="table table-sm table-hover">
                        <thead>
                        <tr>
                            <th class="text-xs-center output-active"></th>
                            <th class="text-xs-center output-index">#</th>
                            <th class="text-xs-center output-number">Type</th>
                            <th class="text-xs-center output-start">Start</th>
                            <th class="text-xs-center output-count">Count</th>
                            <th class="text-xs-center output-label" >Label</th>
                            <th class="text-xs-center output-action p-r-1" style="width: 45px"></th>
                        </tr>
                        </thead>
                        <tbody>
                            <tr is="output" v-for="(index, output) in outputs" :index="index" :output.sync="output"></tr>
                        </tbody>
                    </table>
                </div>
        </form>

    </div>
    </div>
</template>

<script>
import Switch from "../shared/switch.vue";
import Output from "./fpd-output.vue";
import FormInput from "../shared/input.vue";
import outputMixin from "../../mixins/outputMixin";

export default {
    mixins: [outputMixin],
    components: { Switch, Output, FormInput },
    props: [],
    data() {
        return {
             incrementStart: true,
             defStart: 1,
             defType: 'pixelnet',
             defCount: 512,
             outputs: []
        };
    },
    ready() {},
    methods: {
        assembleNewOutput(last) {

            return {
                active: true,
                start: this.incrementStart && last ? (last.start + last.count) : 1,
                count: this.defCount,
                type: this.defType,
                label: null
            };
        }
    },
    computed: {

    },
    events: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

</style>

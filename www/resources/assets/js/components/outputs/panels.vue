<template>
<div id="panel-settings" class="outputs-container container-fluid box">   
    <div class="box-header">
        <h4 class="box-title"><span class="semi-bold">LED Panel</span> Outputs</h4>     
    </div>
    <div class="box-tool">
        <div class="enable-output">
           <small>Enable Outputs </small>
           <switch :model.sync="enable_panels"></switch>
        </div>
    </div>
    <div class="box-body">
        <form>
            <div class="output-actions">
                <div class="filter-row">
                    <div class="filter">
                         <form-input 
                          :model.sync="defStart"
                          type="number"
                          label="Start Channel"
                          size="sm"
                          state-icon
                          @change="updatePanels">
                        </form-input>
                    </div>
                   
                    <div class="filter">
                        <div class="form-group">
                            <label for="" class="control-label">Panel Size</label>
                            <div class="input">
                                <select v-model="size" class="c-select" @change="updatePanels" style="padding-top: 0.25rem; padding-bottom: 0.275rem;">
                                    <option value="32x16">32x16</option>
                                    <option value="32x32">32x32</option>
                                </select>  
                            </div>
                        </div>
                    </div>
                    <div class="filter">
                        <div class="form-group">
                            <label for="" class="control-label">Panel Layout</label>
                            <div class="input">
                                <select v-model="layout" class="c-select" @change="updatePanels" style="padding-top: 0.25rem; padding-bottom: 0.275rem;">
                                    <option v-for="layout in layouts" :value="$index">{{layout.name}}</option>
                                    
                                </select>  
                            </div>
                        </div>
                    </div>
                    <div class="filter">
                        <div class="form-group">
                            <label for="" class="control-label">Start Corner</label>
                            <div class="input">
                                <select v-model="corner" class="c-select" style="padding-top: 0.25rem; padding-bottom: 0.275rem;">
                                    <option value="top-left">Top Left</option>
                                    <option value="bottom-left">Bottom Left</option>
                                </select>  
                            </div>
                        </div>
                    </div>
                    <div class="filter">
                        <div class="form-group">
                            <label for="" class="control-label">Color Order</label>
                            <div class="input">
                                <select v-model="color" class="c-select" style="padding-top: 0.25rem; padding-bottom: 0.275rem;">
                                    <option value="rgb">RGB</option>
                                    <option value="rbg">RBG</option>
                                    <option value="grb">GRB</option>
                                    <option value="gbr">GBR</option>
                                    <option value="brg">BRG</option>
                                    <option value="bgr">BGR</option>
                                </select>  
                            </div>
                        </div>
                    </div>
                    <div class="filter">
                        <button @click="savePanels" type="button" class="button btn-primary pull-xs-right m-a-0">Save Layout</button>
                    </div>
                </div>
              
            </div>
            <div class="led-panels {{sizeClass}}">
                <led-panel v-for="(index, panel) in panels" :index="index" :panel="panel" :layout="layouts[layout]" :size="size"></tr>
            </div>
        </form>
    </div>
</div>
</template>

<script>
import Switch from "../shared/switch.vue";
import FormInput from "../shared/input.vue";
import outputMixin from "../../mixins/outputMixin";
import LedPanel from "./led-panel.vue";

const PANEL = {
    id: 1,
    position: {
        x: 0,
        y: 0
    },
    output: 1,
    panel: 1,
    orientation: 'up',
    start: 1,
    end: 1536
};

//const LAYOUTS = ['1x1', '1x2', '1x3', '1x4', '2x1', '2x2', '3x1', '4x1'];
const LAYOUTS = [
    {name:'1x1', panels: 1},
    {name:'1x2', panels: 2},
    {name:'1x3', panels: 3},
    {name:'1x4', panels: 4},
    {name:'2x1', panels: 2},
    {name:'2x2', panels: 4},
    {name:'3x1', panels: 3},
    {name:'4x1', panels: 4},
];

// Max outputs per device & max panels per output
const MaxPanels = {
    BBB : {
        outputs: 8,
        panels: 8
    },
    pi : {
        outputs: 3,
        panels: 8
    }
};

// pixels per panel
const PPP = {
    '32x16' : 1536,
    '32x32' : 3072
}

function getLayouts(device = 'pi') {
    let max = MaxPanels[device].outputs * MaxPanels[device].panels;
    let layouts = [];

    for(let r = 1; r <= max; r++) {
        for (let c = 1; c <= max; c++) {
            if ((r * c) <= max) {
                let layout = { 
                    name: `${r}x${c}`, 
                    panels: (r * c), 
                    class: r > 1 ? `panel${r}-${c} panel-col-${c}` : `panel${r}-${c}`,


                };
                layouts.push(layout);
            }
        }
    }
    return layouts;
}

export default {
    mixins: [outputMixin],
    components: { Switch, LedPanel, FormInput },
    data() {
        return {
            defStart: 1,
            layouts: getLayouts('BBB'),
            layout: "0",
            size: '32x16',
            corner: 'top-left',
            color: 'rgb',
            panels: [ PANEL ]
            
        };
    },
    ready() {
        console.log(this.layouts);
    },
    methods: {
        updatePanels() {
            let layout = this.layouts[this.layout];
            let numPanels = layout.panels;
            let count = this.panels.length;
            let cols = parseInt(layout.name.charAt(0), 10);
            let rows = parseInt(layout.name.charAt(2), 10);
            let panels = [];


            for(let i = 1; i <= numPanels; i++) {
                let panel = Object.assign({}, PANEL);
                panel.id = i;
                panel.start = i > 1 ? ( PPP[this.size] * (i-1) ) + this.defStart : this.defStart;
                panel.end = PPP[this.size] + (panel.start-1);

                if(cols > 1) {
                    panel.position.x = (i - 1) % cols;
                } 
                if(rows > 1) {
                    panel.position.y = (i - 1) % rows;
                }     
                panels.push(panel);            
            }

            this.panels = panels;
            // if(numPanels < count) {
            //     let diff = count - numPanels; 
            //     this.panels.splice(numPanels, diff);
            //     return;
            // }
            // if(numPanels > count) {
            //     let diff = numPanels - count;
                
            //     for(let i = 0; i <= diff; i++) {
            //         let panel = Object.assign({}, PANEL);
            //         panel.id = i == 0 ? count++ : count + i;
            //         panel.start = PPP[this.size] * (count + i);
            //         panel.end = PPP[this.size] * (count + i);

            //         if(cols > 1) {
            //             panel.position.x = diff;
            //         } 
            //         if(rows > 1) {
            //             panel.position.x = diff;
            //         }
            //        this.panels.push(panel);

            //     } 
            // }
            console.log(this.panels);
        }      
    },
    computed: {
        sizeClass() {
            let name = this.layouts[this.layout].name.replace('x', '-');
            return `layout${name}`;
        }
    }
    

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

.led-panels {
     lost-utility: clearfix;
    //lost-flex-container: row;
    //max-width: 1200px;
    margin: 0 auto;
    // &[class*="layout1"] {
    //     max-width:300px;
    // }
    // &[class*="layout2"] {
    //     max-width:600px;
    // }
    // &[class*="layout3"] {
    //     max-width: 900px;
    // }
}
</style>
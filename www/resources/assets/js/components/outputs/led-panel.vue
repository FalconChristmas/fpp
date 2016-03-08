<template>
    <div class="led-panel" :class="sizeClasses">
        <popover position="top" trigger="['focus']">
            <div class="led-panel-content">
                <div class="panel-info">
                    <span class="panel-info-label">Channels:</span> {{panel.start}} <span class="end">- {{panel.end}}</span>
                </div>
                <div class="panel-info output">
                    <span class="panel-info-label">Output:</span> {{panel.output }}
                </div>
                <div class="panel-info panel">
                    <span class="panel-info-label">Panel:</span> {{panel.panel}}
                </div>
            </div>
            <div slot="content">
               <div style="text-align: center; font-size: 12px; padding-bottom: 5px;">Channels: {{panel.start}} to {{panel.end}}</div>
                <div class="form-group">
                    <label for="" class="control-label" style="text-transform: uppercase; font-size: 12px; margin-right: 5px;">Output Number</label>
                    <select v-model="panel.output" class="c-select" style="padding-top: 0.25rem; padding-bottom: 0.275rem;">
                        <option value="1">1</option>
                        <option value="2">2</option>
                        <option value="3">3</option>
                        <option value="4">4</option>
                        <option value="5">5</option>
                        <option value="6">6</option>
                        <option value="7">7</option>
                        <option value="8">8</option>
                    </select>  
                </div>
                <div class="form-group">
                    <label for="" class="control-label" style="text-transform: uppercase; font-size: 12px; margin-right: 5px;">Panel Number</label>
                    <select v-model="panel.panel" class="c-select" style="padding-top: 0.25rem; padding-bottom: 0.275rem;">
                        <option value="1">1</option>
                        <option value="2">2</option>
                        <option value="3">3</option>
                        <option value="4">4</option>
                        <option value="5">5</option>
                        <option value="6">6</option>
                        <option value="7">7</option>
                        <option value="8">8</option>
                    </select>  
                </div>
                <div class="form-group">
                    <label for="" class="control-label" style="text-transform: uppercase; font-size: 12px; margin-right: 5px;">Panel Orientation</label>
                    <select v-model="panel.orientation" class="c-select" style="padding-top: 0.25rem; padding-bottom: 0.275rem;">
                        <option value="up">Up</option>
                        <option value="down">Down</option>
                        <option value="left">Left</option>
                        <option value="right">Right</option>
                       
                    </select>  
                </div>
            </div>
        </popover>
    </div>
</template>

<script>
import Popover from '../shared/popover.vue';

export default {
    components: { Popover },
    props: ['panel', 'layout', 'size', 'index'],
    data() {
        return {
           
        };
    },
    ready() {
        
    },
    methods: {},
    events: {

    },
    computed: {
        sizeClasses() {
            let orientation = this.panel.orientation + '-orient';
            let layout = this.layout.class.split(' ');
            return [ 
                this.size == '32x32' ? 'panel32' : '', 
                orientation
                ].concat(layout);

        },
        orientation() {
            return this.panel.orientation + '-orient';
        }
    }

}
</script>

<style lang="sass">
@import "resources/assets/sass/_vars.scss";
@import "resources/assets/sass/_mixins.scss";
@lost gutter 5px;
@lost flexbox no-flex;
.led-panel {
    position: relative;
    background-color: $secondary-color;
    border: 1px solid #ddd;
    margin-bottom: 5px;
    background-repeat: no-repeat;
    background-position: center;
    background-size: 15%;
    //min-width: 150px;
    &:before {
        display: block;
        content: "";
        width: 100%;
        padding-top: 50%;

    }
    
    &.left-orient,
    &.right-orient {
        &:before {
            padding-top: 150%;
        }
    }
    &.left-orient {
        background-image: url('/img/arrow-left.svg');
    }
    &.right-orient {
        background-image: url('/img/arrow-right.svg');
    }
    &.up-orient {
        background-image: url('/img/arrow-up.svg');
    }
    &.down-orient {
        background-image: url('/img/arrow-down.svg');
    }

    &.panel32 {
        &:before {
            padding-top: 100%;
        }
    }


    &-content {
        position: absolute;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        padding: 10px;
        overflow: hidden;
        text-align: center;

    }

    .panel-info {
        overflow: hidden;
        line-height: 1.2;
        text-transform: uppercase;
        font-size: 12px;
        color: darken($primary-color, 10%);
        display: inline-block;
        text-align: center;
        margin: 2px 5px;
        font-weight: bold;
        &-label {
            color: $primary-color;
            display: block;
            font-size: 10px;
            font-weight: normal;
        }

    }
   
    // &.panel2-1 {
    //     lost-column: 1/2;
    // }
    // &.panel3-1 {
    //     lost-column: 1/3;
    // }
    // &.panel4-1 {
    //     lost-column: 1/4;
    // }
    // &.panel2-2 {
    //     lost-waffle: 1/2;
    // }
    
    @for $i from 2 to 64 {
        &.panel-col-#{$i} {
            lost-waffle: unquote("1/#{$i}");

            @if $i > 8 {
                .led-panel-content {
                    padding: 3px 0;
                    text-align: center;
                    
                }
            }
            @if $i > 10 {
                .led-panel-content {
                    .panel-info {
                        span { display: none; }
                        &.output, 
                        &.panel { display: none;}
                    }
                }
            }
        }
    }
    

   
}
</style>
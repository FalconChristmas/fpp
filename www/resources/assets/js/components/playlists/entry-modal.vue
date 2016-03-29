<template>
     <modal id="entry-modal" size="md" :fade="true">
          <div slot="modal-header">
             <h5>Add Playlist <span class="semi-bold">Entry</span></h5>
          </div>
          <div slot="modal-body">
            <div class="entry-group">
                <div class="form-group-default">
                    <label>Entry Type</label>
                    <select v-model="type" class="c-select" style="width: 100%;">
                        <option v-for="(key, val) in types" :value="key">{{val}}</option>
                    </select>
                </div>
            </div>
            <div class="form-group-default" v-if="type == 'both' || type == 'media'">
                <label>Media</label>
                <select v-model="media" class="c-select" style="width: 100%;">
                    <option v-if="!mediaFiles" value="">No media files found</option>
                    <option v-if="mediaFiles" v-for="song in mediaFiles" :value="song">{{song}}</option>
                </select>
            </div>
            <div class="form-group-default" v-if="type == 'both' || type == 'sequence'">
                <label>Sequence</label>
                <select v-model="sequence" class="c-select" style="width: 100%;">
                    <option v-if="!sequenceFiles" value="">No sequences found</option>
                    <option v-if="sequenceFiles" v-for="sequence in sequenceFiles" :value="sequence">{{sequence}}</option>
                </select>
            </div>
            <div class="form-group-default" v-if="type == 'pause'">
                <label>Pause Length</label>
                <span class="help">(in seconds)</span>
                <input type="number" v-model="pause" class="form-control">
            </div>
            <div class="form-group-default" v-if="type == 'event'">
                <label>Event</label>
                 <select v-model="event" class="c-select" style="width: 100%;">
                    <option v-if="!events" value="">No events configured</option>
                    <option v-if="events" v-for="event in events" :value="event">{{event}}</option>
                </select>
            </div>
            <div class="form-group-default" v-if="type == 'effect'">
                <label>Effect</label>
                 <select v-model="effect" class="c-select" style="width: 100%;">
                    <option v-if="!effects" value="">No effects found</option>
                    <option v-if="effects" v-for="effect in effects" :value="event">{{effect}}</option>
                </select>
            </div>
            <div class="form-group-default" v-if="type == 'volume'">
                <label>Volume</label>
                <range-slider :min="0" :max="100" :step="1" :value.sync="volume"><span slot="right">{{volume}}</span></range-slider>
            </div>
            <div class="form-group-default" v-if="type == 'plugin'">
                <label>Plugin Data</label>
                <input type="text" v-model="plugin" class="form-control">
            </div>
            <div class="form-group-default" v-if="type == 'pixelOverlay'">
                <label>Model Name</label>
                <select v-model="overlayModelName" class="c-select" style="width: 100%;">
                    <option v-if="!models" value="">No Models found</option>
                    <option v-if="models" v-for="model in models" :value="model">{{model}}</option>
                </select>
            </div>
            <div class="form-group-default" v-if="type == 'pixelOverlay'">
                <label>Action</label>
                <select v-model="overlayAction" class="c-select" style="width: 100%;">
                    <option value="enabled">Enabled</option>
                    <option value="disabled">Disabled</option>
                    <option value="value">Value</option>
                </select>
            </div>
            <div class="row" v-if="type == 'pixelOverlay'">
                <div class="col-sm-4" v-if="overlayAction == 'value'">
                    <div class="form-group-default">
                        <label>Start Channel</label>
                        <input type="number" v-model="startChannel">
                    </div>
                </div>
                <div class="col-sm-4" v-if="overlayAction == 'value'">
                    <div class="form-group-default">
                        <label>End Channel</label>
                        <input type="number" v-model="endChannel">
                    </div>
                </div>
                <div class="col-sm-4" v-if="overlayAction == 'value'">
                    <div class="form-group-default">
                        <label>Value</label>
                        <input type="number" v-model="modelValue">
                    </div>
                </div>
                
            </div>
            
          </div>
          <div slot="modal-footer">
            <button class="button alert m-b-0" v-on:click="$broadcast('hide::modal', 'entry-modal')">Cancel</button>
            <button class="button primary m-b-0" v-on:click="$dispatch('add')">Add Entry</button>
          </div>
    </modal>
</template>

<script>
import Modal from "../shared/modal.vue";
import RangeSlider from "../shared/range-slider.vue";

export default {
    components: { Modal, RangeSlider },
    props: {
        types: {
            type: Object,
            required: true,
        },
        mediaFiles: {
            type: Array,
        },
        sequenceFiles: {
            type: Array,
        },
        events: {
            type: Array
        }
    },
    data() {
        return {
            type: null,
            plugin: null,
            event: null,
            sequence: null,
            media: null,
            pause: null,
            volume: null,
        };
    },

    events: {
        add() {
            let entry = {
                type: this.type,
                plugin: this.plugin,
                event: this.event,
                sequence: this.sequence,
                media: this.media,
                pause: this.pause,
                volume: this.volume
            };
            this.$dispatch('add-entry', entry);
            this.$broadcast('hide::modal', 'entry-modal')
        }
    }
}
</script>

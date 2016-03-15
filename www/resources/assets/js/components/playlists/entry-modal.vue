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
            <div class="form-group-default" v-if="type == 'b' || type == 'm'">
                <label>Media</label>
                <select v-model="media" class="c-select" style="width: 100%;">
                    <option v-if="!mediaFiles" value="">No media files found</option>
                    <option v-if="mediaFiles" v-for="song in mediaFiles" :value="song">{{song}}</option>
                </select>
            </div>
            <div class="form-group-default" v-if="type == 'b' || type == 's'">
                <label>Sequence</label>
                <select v-model="sequence" class="c-select" style="width: 100%;">
                    <option v-if="!sequenceFiles" value="">No sequences found</option>
                    <option v-if="sequenceFiles" v-for="sequence in sequenceFiles" :value="sequence">{{sequence}}</option>
                </select>
            </div>
            <div class="form-group-default" v-if="type == 'p'">
                <label>Pause Length</label>
                <span class="help">(in seconds)</span>
                <input type="number" v-model="pause" class="form-control">
            </div>
            <div class="form-group-default" v-if="type == 'e'">
                <label>Event</label>
                 <select v-model="event" class="c-select" style="width: 100%;">
                    <option v-if="!events" value="">No events configured</option>
                    <option v-if="events" v-for="event in events" :value="event">{{event}}</option>
                </select>
            </div>
            <div class="form-group-default" v-if="type == 'P'">
                <label>Plugin Data</label>
                <input type="text" v-model="plugin" class="form-control">
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

export default {
    components: { Modal },
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
            pause: null
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
                pause: this.pause
            };
            this.$dispatch('add-entry', entry);
            this.$broadcast('hide::modal', 'entry-modal')
        }
    }
}
</script>

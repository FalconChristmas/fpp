<template>
    <modal id="output-modal" size="md" :fade="true">
          <div slot="modal-header">
             <h6 class="modal-title">Add {{ type | capitalize }} <span class="semi-bold">Outputs</span></h6>
          </div>
          <div slot="modal-body">
            <div class="switches">

            </div>
            <div class="fields">
                <div class="field" v-for="field in fields">
                    <form-input
                      :model.sync="field.model"
                      :type="field.type"
                      :label="field.name"
                      size="sm"
                      state-icon>
                    </form-input>
                </div>
            </div>
          </div>
          <div slot="modal-footer">
            <button class="button gradient m-b-0" v-on:click="$broadcast('hide::modal', 'output-modal')">Cancel</button>
            <button class="button gradient submit m-b-0" v-on:click="$dispatch('add')">Add</button>
          </div>
    </modal>
</template>

<script>
import Modal from "../shared/modal.vue";
import Switch from "../shared/switch.vue";
import FormInput from "../shared/input.vue";

export default {
    components: { Modal, Switch, FormInput },
    props: {
        type: {
            type: String
        },
        switches: {
            type: Object
        },
        fields: {
            type: Array,
            required: true
        }
    },
    data() {
        return {

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
            this.$dispatch('add-output', entry);
            this.$broadcast('hide::modal', 'output-modal')
        }
    }
}
</script>

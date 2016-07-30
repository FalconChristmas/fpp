<template>
    <modal id="output-modal" size="md" :fade="true">
          <div slot="modal-header">
             <h6 class="modal-title">Add {{ type | capitalize }} <span class="semi-bold">Outputs</span></h6>
          </div>
          <div slot="modal-body">
            <div class="switches switch-row">
              <div class="field text-xs-center" v-for="field in switches">
                  <span>{{field.name}}</span>
                  <switch :model.sync="field.model" size="small"></switch>
              </div>
            </div>
            <hr>
            <div class="fields row">
                <div :class="['field', field.classes]" v-for="field in fields">
                    <form-input
                      v-if="field.type !== 'select'"
                      :model.sync="field.model"
                      :type="field.type"
                      :label="field.name"
                      size="sm"
                      state-icon>
                    </form-input>
                    <div v-else class="form-group">
                        <label for="" class="control-label">{{field.name}}</label>
                        <div class="input">
                          <select class="c-select" style="width: 100%;" v-model="field.model">
                            <option v-for="option in field.options" value="{{option}}">{{option | capitalize}}</option>
                          </select>
                        </div>
                    </div>
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
            type: Array,
            required: true
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
<style lang="sass">
.modal .switch-row {
  display: flex;
  justify-content: space-between;
  align-items: center;

  .field span {
    display: inline-block;
    vertical-align: middle;
    margin-bottom: 10px;
  }
}
</style>

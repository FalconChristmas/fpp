<template>
    <tr class="pdmx output" :class="{ 'disabled' : !output.active }" transition="fade">
        <td class="output-active text-xs-center">
            <switch :model.sync="output.active" size="small"></switch>
           
        </td>
        <td class="output-index text-xs-center">{{ index + 1 }}</td>
         <td class="output-type text-xs-center">
            <select class="c-select transparent" v-model="output.type">
                <option value="pixelnet">Pixelnet</option>
                <option value="dmx">DMX</option>
            </select>
        </td>
        <td class="output-start text-xs-center">
            <input type="number" v-model="output.start" class="form-control transparent text-xs-center" number>
        </td>
          <td class="output-count text-xs-center">
            <input type="number" v-model="output.count" class="form-control transparent text-xs-center" number>
        </td>
        <td class="output-label text-xs-center"><input type="text" v-model="output.label" class="form-control transparent"></td>
        <td class="output-action text-xs-center p-r-1">
            <button type="button" class="close" aria-label="Close" @click="removeOutput">
                <span aria-hidden="true">&times;</span>
            </button>
        </td>
    </tr>
</template>

<script>
import Switch from "../shared/switch.vue";
export default {
    components: { Switch },
    props: [ 'output', 'index' ],
    data() {
        return {};
    },
    ready() {},
    methods: {
        removeOutput() {
            this.$dispatch('remove-output', this.index);
        }
    },
    events: {},


}
</script>

<style lang="sass">
  
</style>

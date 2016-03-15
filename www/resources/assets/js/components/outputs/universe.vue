<template>
    <tr class="universe output" :class="{ 'disabled' : !universe.active }">
        <td class="universe-active text-xs-center">
            <switch :model.sync="universe.active" size="small"></switch>
           
        </td>
        <td class="universe-index text-xs-center">{{ index + 1 }}</td>
        <td class="universe-number">
            <input type="number" v-model="universe.universe" class="form-control transparent text-xs-center" number>
        </td>
        <td class="universe-start text-xs-center">
            <input type="number" v-model="universe.start" class="form-control transparent text-xs-center" number>
        </td>
        <td class="universe-size text-xs-center">
            <input type="number" v-model="universe.size" max="512" class="form-control transparent text-xs-center" number>
        </td>
        <td class="universe-type text-xs-center">
            <select class="c-select transparent" v-model="universe.type">
                <option value="unicast">Unicast</option>
                <option value="multicast">Multicast</option>
            </select>
        </td>
        <td class="universe-address text-xs-center"><input type="text" v-model="universe.address" :disabled="universe.type == 'multicast'" placeholder="xxx.xxx.xxx.xxx" class="form-control transparent text-xs-center"></td>
        <td class="universe-label text-xs-center"><input type="text" v-model="universe.label" class="form-control transparent"></td>
        <td class="universe-action text-xs-center p-r-1">
            <button type="button" class="close" aria-label="Close" @click="removeOutput('e131', index)">
                <span aria-hidden="true">&times;</span>
            </button>
        </td>
    </tr>
    
</template>

<script>
import Switch from "../shared/switch.vue";
import {
    removeOutput,
    updateOutput
} from "../../state/actions";

export default {
    components: { Switch },
    props: [ 'universe', 'index' ],
    vuex: {
        actions: {
            removeOutput,
            updateOutput
        }
    }

}
</script>

<style lang="sass">
    .universe {
        transition: opacity 0.35s;
        opacity: 1;
        &.disabled {
            opacity: .4;
        }
        &-move {
          transition: transform .5s cubic-bezier(.55,0,.1,1);
        }
    }

    .universe-active,
    .universe-action,
    .universe-index {
        vertical-align: middle !important;

        .checkbox {
            margin-bottom: 0;
        }
    }
</style>

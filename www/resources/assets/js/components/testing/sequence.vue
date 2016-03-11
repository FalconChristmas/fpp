<template>
<div id="sequence-testing-settings" class="page-container container-fluid box">   
     <div class="box-header">
         <h4 class="box-title"><span class="semi-bold">Sequence</span> Testing</h4>     
     </div>
     <div class="box-tool">
         <div class="enable-output">
           <small>Enable Test Mode </small>
           <switch :model="enabled" :on-change="onToggle"></switch>
         </div>
     </div>
     <div class="box-body">
        <form>
        </form>
     </div>
</div>
</template>

<script>
import Switch from "../shared/switch.vue";

export default {
    components: { Switch },
    props: [],
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {},
    vuex: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

</style>
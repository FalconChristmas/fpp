<template>
    <div id="dashboard" class="container-fluid">
        <div class="row">
            <div class="col-md-4">
                <div class="box">
                    <div class="box-header">
                         <h4>FPPD Status</h4>
                    </div>
                   
                   
                </div>
            </div>
            <div class="col-md-4">
                <div class="box">
                     <div class="box-header">
                         <h4>FPPD Mode</h4>
                    </div>
                </div>
            </div>
            <div class="col-md-4">
                <div class="box widget p-a-1">
                    <div class="pull-xs-left m-r-1">
                        <span class="rounded bg-success widget-icon">
                            <i class="ion-clock"></i>
                        </span>
                    </div>
                    <div class="clear">
                        <h4 class="text-lg">12:36 p.m.</h4>
                        <small class="text-muted">FPPD Time</small>
                    </div>
                    
                </div>
            </div>
            
        </div>
        <div class="row">
            <div class="col-md-4">
                <div class="card">
                    <div class="card-header">
                        Player
                    </div>
                    <div class="card-block">
                        
                    </div>
                </div>
            </div>
        </div>
        
    </div>
</template>

<script>
export default {
    components: {},
    props: [],
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

</style>
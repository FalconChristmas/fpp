<template>
   <div class="box panel widget widget-multisync">
        <div class="panel-heading">
            <div class="panel-title">MultiSync</div>
            <div class="panel-controls">
                <ul>
                  <li><a href="#" class="portlet-refresh" data-toggle="collapse"><i class="portlet-icon pg-refresh"></i></a>
                  </li>
                  
                </ul>
            </div>
        </div>
        <div class="panel-body p-a-0">
            <div class="multisync-items">
                <div class="table-responsive">
                    <table class="table table-sm table-striped m-t-0">
                        <thead>
                            <tr>
                                <th></th>
                                <th>System Name</th>
                                <th>IP Address</th>
                                <th>Platform</th>
                                <th>Mode</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr>
                            <td colspan="5" class="text-center all-caps fs-12 ls-2 text-muted p-a-1">No Systems Found</td>
                            </tr>
                        </tbody>
                    </table>
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
    
.widget-multisync {
    .table {
        thead {
            th {
                font-size: 9px;
                background: #e6e6e6;//#b1b1b1;
                font-weight: normal;
                border-bottom: 0;
                //color: white;
            }
        }
    }
}
</style>
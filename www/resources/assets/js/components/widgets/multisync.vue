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
                    <table class="table table-sm table-striped m-t-0 m-b-0">
                        <thead>
                            <tr>
                                <th class="">Local</th>
                                <th class="">System Name</th>
                                <th class="text-center">IP Address</th>
                                <th class="text-center">Platform</th>
                                <th class="text-center mode">Mode</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr>
                                <td class="text-center"><i class="ion-checkmark text-success"></i></td>
                                <td class="">FPP</td>
                                <td class="text-center">192.168.1.1</td>
                                <td class="text-center">Raspberry Pi</td>
                                <td class="text-center">Player</td>
                            </tr>
                            <tr>
                                <td class="text-center"><i class="ion-close text-danger"></i></td>
                                <td class="">FPP2</td>
                                <td class="text-center">192.168.1.2</td>
                                <td class="text-center">Raspberry Pi</td>
                                <td class="text-center">Player</td>
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
                background: #f1f1f1;//#b1b1b1;
                font-weight: normal;
                border-bottom: 0;
                border-top: 1px solid #e6e6e6;
                border-bottom: 1px solid #e6e6e6;
                //color: white;
                &.mode {
                    padding-right: 0 !important;
                }
            }
        }
    }
}
</style>

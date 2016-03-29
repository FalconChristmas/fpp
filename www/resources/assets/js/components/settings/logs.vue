<template>
  <div id="log-settings" class="settings-container container-fluid box p-a-2">
    <h4><span class="semi-bold">Log</span> Settings</h4>
    <hr>
      <form>
        <div class="form-group row">
          <div class="col-sm-7">
            <label for="" class="form-control-label">Log Level</label>
              <p class="text-muted  hidden-sm-down">Select how detailed the output logs should be.</p>
          </div>
          <div class="col-sm-2 col-sm-offset-1 text-xs-right">
              <select class="c-select" v-model="log_level">
                <option value="warn">Warn</option>
                <option value="info">Info</option>
                <option value="debug">Debug</option>
                <option value="excess">Excessive</option>
              </select>
            </div>

        </div>
        <div class="form-group row">
          <div class="col-sm-7">
            <label for="" class="form-control-label">Log Mask</label>
              <p class="text-muted  hidden-sm-down">Select the logs to be output at the selected Log Level.</p>
          </div>
          <div class="col-sm-2 col-sm-offset-1 text-xs-right">
              <select class="c-select" v-model="log_mask">
                <option value="warn">ALL</option>
                <option value="info">Most</option>
              </select>
            </div>

        </div>

  <!--       <div class="row">
          <div class="col-xs-12">
            <h5 class="semi-bold">Log Mask</h5>
          </div>
        </div>

        <div class="form-group row">
          <div class="col-sm-6">
            <div class="row">
                <div class="col-sm-7">
                  <label for="" class="form-control-label">ALL</label>
                    <p class="text-muted  hidden-sm-down">The ALL meta-value enables all debug logs at the selected level. This can be very verbose and is normally only recommended at the Warn or Info levels.</p>
                </div>
                <div class="col-sm-2 col-sm-offset-1 text-xs-right">
                    <switch :model.sync="mask_all"></switch>
                </div>
            </div>
          </div>
          <div class="col-sm-6">
            <div class="row">
                <div class="col-sm-7">
                  <label for="" class="form-control-label">Most (default)</label>
                    <p class="text-muted  hidden-sm-down">The Most meta-value enables all debug logs except for Channel Data.</p>
                </div>
                <div class="col-sm-2 col-sm-offset-1 text-xs-right">
                    <switch :model.sync="mask_most"></switch>
                </div>
            </div>
          </div>
        </div> -->

        <div class="log-masks" v-show="log_mask">
          <hr/>
          <div class="form-group row">
            <div class="col-sm-6">
              <div class="row">
                  <div class="col-sm-9">
                    <label for="" class="form-control-label">Channel Data <span class="hint--top label-help" data-hint="Log every time that channel data is sent out to controllers"><i class="ion-help-circled"></i></span></label>
                  </div>
                  <div class="col-sm-2 p-t-5 text-xs-right">
                      <switch :model.sync="mask_channel_data"></switch>
                  </div>
              </div>
            </div>
            <div class="col-sm-6">
              <div class="row">
                  <div class="col-sm-9">
                    <label for="" class="form-control-label">GPIO <span class="hint--top label-help" data-hint="Log GPIO Input events"><i class="ion-help-circled"></i></span></label>
                  </div>
                  <div class="col-sm-2 p-t-5 text-xs-right">
                      <switch :model.sync="mask_gpio"></switch>
                  </div>
              </div>
            </div>
          </div>

            <div class="form-group row">
            <div class="col-sm-6">
              <div class="row">
                  <div class="col-sm-9">
                    <label for="" class="form-control-label">Channel Outputs <span class="hint--top label-help" data-hint="Log info about the Channel Outputs themselves"><i class="ion-help-circled"></i></span></label>
                  </div>
                  <div class="col-sm-2 p-t-5 text-xs-right">
                      <switch :model.sync="mask_channel_outputs"></switch>
                  </div>
              </div>
            </div>
            <div class="col-sm-6">
              <div class="row">
                  <div class="col-sm-9">
                    <label for="" class="form-control-label">Media Outputs <span class="hint--top label-help" data-hint="Log info about the media players that FPP uses."><i class="ion-help-circled"></i></span></label>
                  </div>
                  <div class="col-sm-2 p-t-5 text-xs-right">
                      <switch :model.sync="mask_media"></switch>
                  </div>
              </div>
            </div>
          </div>
        </div>



      </form>
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
    events: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

    .label-help {
      i {
        cursor: pointer;
        opacity: .5;
        font-size: 16px;
        margin-left: 5px;
      }
    }
</style>

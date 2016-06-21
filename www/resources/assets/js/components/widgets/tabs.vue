<template>
  <div class="panel widget tabs-widget box">
    <button class="add-tab-item button small gradient" @click="handleAdd">+ Add {{ activeTab }}</button>
    <Tabs classes="nav-tabs-simple" @tab-changed="onTabChange">
        <Tab id="events" header="Events"><events :events="events"></events></tab>
        <Tab id="effects" header="Effects">Effects Tab</tab>
    </Tabs>
  </div>
</template>

<script>
import Events from "./events.vue";
import Tabs from "../shared/tabset.vue";
import Tab from "../shared/tabset-tab.vue";

export default {
  components: { Tabs, Tab, Events },
  data() {
    return {
      activeTab: null,
      events: [
        {
          id: '1/2',
          name: 'An event Name',
          script: 'EventScript.sh',
          effect: 'MyNewEffect.eseq',
          channel: ''
        }

      ]
    }
  },
  methods: {
    onTabChange(tab) {
      this.activeTab = tab.header.slice(0, -1);
    },
    handleAdd() {
      // dispatch action to show modal
    }
  }
}

</script>

<style lang="sass">
@import "resources/assets/sass/_vars.scss";
@import "resources/assets/sass/_mixins.scss";

.tabs-widget {
  position: relative;
  .add-tab-item {
    position: absolute;
    margin: 0;
    top: 10px;
    right: 10px;
  }
  .nav-tabs-simple {
    margin-bottom: 0 !important;

    &:after {
      border-bottom: 0;
      bottom: 0;
    }
    > li {
      margin-bottom: 0;

      > a {
        padding: 10px 20px 11px;
      }
      &:after {
        position: absolute;
        bottom: 0;
      }

      &.active,
      &:hover {
        &:after {
          background-color: $highlight-color;
        }
      }
    }
  }
  .tab-content {
    padding: 0;
  }

}
</style>

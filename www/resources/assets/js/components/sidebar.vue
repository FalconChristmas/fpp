<template>
    <aside id="sidebar-left" class="sidebar affix">

    <div class="logo">
        <img src="/img/logo.svg">
    </div>

    <div class="sidebar-menu">
        <div class="menu-section">
            <h4>Control</h4>
            <ul class="nav">
                <li><a v-link="{ name: 'dashboard', exact: true, activeClass: 'active' }">Dashboard</a></li>
                <li><a v-link="{name: 'testing', activeClass: 'active'}">Display Testing</a></li>

            </ul>
        </div>

        <div class="menu-section">
            <h4>Content</h4>
            <ul class="nav">
                <li><a v-link="{ name: 'files',  activeClass: 'active' }">File Manager</a></li>
                <li><a v-link="{ name: 'playlists', activeClass: 'active' }">Playlists</a></li>
                <li><a v-link="{ name: 'schedule', activeClass: 'active' }">Schedule</a></li>
                <li><a v-link="{ name: 'plugins', activeClass: 'active' }">Plugins</a></li>
            </ul>

        </div>

        <div class="menu-section">
            <h4>Configure</h4>
            <ul class="nav">
                <li><a v-link="{name: 'outputs', activeClass: 'active'}">Outputs</a></li>
                <li><a v-link="{name: 'gpio', activeClass: 'active'}">GPIO</a></li>
                <li><a v-link="{name: 'overlays', activeClass: 'active'}">Overlay Models</a></li>

                <li><a v-link="{name: 'settings', activeClass: 'active'}">Settings</a></li>
            </ul>
        </div>
    </div>
    <div class="device-stats m-t-30">
        <div class="device-stats-cpu">
            <h6>CPU Usage <span class="pull-xs-right">25%</span></h6>
            <progress class="progress progress-info" value="25" max="100">25%</progress>
        </div>
         <div class="device-stats-ram">
            <h6>Memory Usage <span class="pull-xs-right">65%</span></h6>
            <progress class="progress progress-warning" value="65" max="100">65%</progress>
        </div>
         <div class="device-stats-disk">
            <h6>Disk Usage <span class="pull-xs-right">15%</span></h6>
            <progress class="progress progress-complete" value="15" max="100">15%</progress>
        </div>
    </div>
     <div class="device-version text-small text-muted m-a-0 fs-10"><div class="all-caps hint-text">FPP Version:</div> {{ version }}</div>
</aside>

</template>

<script>
export default {
    vuex: {
        getters: {
            version: state => state.shared.version
        }
    }

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

    .sidebar {
      width: $sidebar-width;
      position: fixed;
      height: 100%;
      background: $sidebar-bg-color;
      z-index: 10;
      padding: 20px;
      transition: all 0.25s;
      left: -#{$sidebar-width};
      display: flex;
      flex-direction: column;
      align-items: flex-start;
      justify-content: space-between;
      overflow-y: auto;

      @include breakpoint($tablet-pt-min) {
        left: 0;
        width: $sidebar-width;
      }

      @include breakpoint($desktop-min) {
        width: $sidebar-width-desktop;
      }

      .logo {
        //margin: 0 0 40px;
        margin: 0 0 20px;
        img {
          width: 100%;
          height: auto;
        }
      }
      h4 {
        text-transform: uppercase;
        color: #999;
        font-size: 10px;
        font-weight: 600;
        letter-spacing: 1px;
        margin: 20px 0 0;
      }

      li {
        padding: 3px 0;
      }
      a {
        color: $sidebar-a-color;
        font-weight: 300;
        font-size: 14px;
        transition: all 0.25s;

          &:hover {
            color: $sidebar-a-color-active;
          }
          &.active {
            color: $sidebar-a-color-active;
            font-weight: 500;
          }
       }

       .sidebar-menu {
            margin-bottom: auto;
       }
       .device-version {
            //position: absolute;
            bottom: 0;
            left: 0;
            width: 100%;
            //padding: 10px;
       }
       .device-stats {
            color: white;
            width: 100%;
            margin: 30px 0 20px;

            h6 {
                color: $sidebar-a-color;
                font-weight: 500;
                font-size: 12px;

            }
       }

    }
</style>

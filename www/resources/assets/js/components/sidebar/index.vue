<template>
    <aside id="sidebar-left" class="sidebar affix">

    <div class="logo">
        <img src="/img/logo.svg">
    </div>

    <div class="sidebar-menu">
        <div class="menu-section">
            <h4>Control</h4>
            <ul class="nav">

            </ul>
        </div>

        <div class="menu-section">
            <h4>Content</h4>
            <ul class="nav">

            </ul>
            
        </div>

        <div class="menu-section">
            <h4>Settings</h4>
            <ul class="nav">

            </ul>
        </div>
    </div>
</aside>

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
    
    .sidebar {
      width: $sidebar-width;
      position: fixed;
      height: 100%;
      background: $sidebar-bg-color;
      z-index: 10;
      padding: 20px;
      transition: all 0.25s;
      left: -#{$sidebar-width};

      @include breakpoint($tablet-pt-min) {
        left: 0;
        width: $sidebar-width;
      }

      @include breakpoint($desktop-min) {
        width: $sidebar-width-desktop;
      }

      .logo {
        margin: 0 0 40px;
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

      a {
        color: $sidebar-a-color;
        font-weight: 300;
        font-size: 14px;
        transition: all 0.25s;

          &:hover {
            color: $sidebar-a-color-active;
          }
       }

    }
</style>
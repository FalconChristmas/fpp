<template>
<fieldset class="form-group {{inputState}}">
    <div class="checkbox" v-for="item in list" v-bind:class="{ 'checkbox-inline': !vertical, disabled: item.disabled }">
        <label v-bind:class="{ 'c-input': custom, 'c-checkbox': custom }">
            <input 
                id="{{item.id}}" 
                type="checkbox" 
                value="{{item.value}}" 
                autocomplete="off" 
                v-model="item.checked" 
                v-bind:disabled="item.disabled">
                <span class="c-indicator" v-if="custom"></span> {{item.text}}
        </label>
    </div>
</fieldset>
</template>

<script>
export default {
  replace: true,
  computed: {
     inputState() {
      return !this.state || this.state === `default` ? `` : `has-${this.state}`
    },
  },
  props: {
    list: {
      type: Array,
      twoWay: true,
      default: [],
      required: true
    },
    model: {
      type: Array,
      twoWay: true,
      default: []
    },
    custom: {
        type: Boolean,
        default: true
    },
    vertical: {
        type: Boolean,
        default: false
    },
    state: {
        type: String,
        default: 'default'
    },
    returnObject: {
      type: Boolean,
      default: false
    },
    },
  watch: {
    list: {
      handler() {
        this.model = []
        this.list.forEach((item) => {
          if (item.checked) {
            if (this.returnObject) {
              this.model.push(item)
            } else {
              this.model.push(item.value)
            }
          }
        })
        // dispatch an event
        this.$dispatch('changed::button-checkbox', this.model)
      },
      deep: true,
    }
  },
  ready() {
    // handle initial selection
    this.list.forEach((item) => {
      if (this.returnObject) {
        this.model.forEach((modelItem) => {
          if (modelItem.value === item.value) {
            item.checked = true
          }
        })
      } else {
         if (this.model.indexOf(item.value) !== -1) {
          item.checked = true
        }
      }
    })
  }
}
</script>

<style lang="sass">

.c-checkbox {
  .c-indicator {
    border-radius: .25rem;
  }

  input:checked ~ .c-indicator {
    background-image: url(data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0idXRmLTgiPz4NCjwhLS0gR2VuZXJhdG9yOiBBZG9iZSBJbGx1c3RyYXRvciAxNy4xLjAsIFNWRyBFeHBvcnQgUGx1Zy1JbiAuIFNWRyBWZXJzaW9uOiA2LjAwIEJ1aWxkIDApICAtLT4NCjwhRE9DVFlQRSBzdmcgUFVCTElDICItLy9XM0MvL0RURCBTVkcgMS4xLy9FTiIgImh0dHA6Ly93d3cudzMub3JnL0dyYXBoaWNzL1NWRy8xLjEvRFREL3N2ZzExLmR0ZCI+DQo8c3ZnIHZlcnNpb249IjEuMSIgaWQ9IkxheWVyXzEiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgeG1sbnM6eGxpbms9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkveGxpbmsiIHg9IjBweCIgeT0iMHB4Ig0KCSB2aWV3Qm94PSIwIDAgOCA4IiBlbmFibGUtYmFja2dyb3VuZD0ibmV3IDAgMCA4IDgiIHhtbDpzcGFjZT0icHJlc2VydmUiPg0KPHBhdGggZmlsbD0iI0ZGRkZGRiIgZD0iTTYuNCwxTDUuNywxLjdMMi45LDQuNUwyLjEsMy43TDEuNCwzTDAsNC40bDAuNywwLjdsMS41LDEuNWwwLjcsMC43bDAuNy0wLjdsMy41LTMuNWwwLjctMC43TDYuNCwxTDYuNCwxeiINCgkvPg0KPC9zdmc+DQo=);
  }

  input:indeterminate ~ .c-indicator {
    background-color: #0074d9;
    background-image: url(data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0idXRmLTgiPz4NCjwhLS0gR2VuZXJhdG9yOiBBZG9iZSBJbGx1c3RyYXRvciAxNy4xLjAsIFNWRyBFeHBvcnQgUGx1Zy1JbiAuIFNWRyBWZXJzaW9uOiA2LjAwIEJ1aWxkIDApICAtLT4NCjwhRE9DVFlQRSBzdmcgUFVCTElDICItLy9XM0MvL0RURCBTVkcgMS4xLy9FTiIgImh0dHA6Ly93d3cudzMub3JnL0dyYXBoaWNzL1NWRy8xLjEvRFREL3N2ZzExLmR0ZCI+DQo8c3ZnIHZlcnNpb249IjEuMSIgaWQ9IkxheWVyXzEiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgeG1sbnM6eGxpbms9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkveGxpbmsiIHg9IjBweCIgeT0iMHB4Ig0KCSB3aWR0aD0iOHB4IiBoZWlnaHQ9IjhweCIgdmlld0JveD0iMCAwIDggOCIgZW5hYmxlLWJhY2tncm91bmQ9Im5ldyAwIDAgOCA4IiB4bWw6c3BhY2U9InByZXNlcnZlIj4NCjxwYXRoIGZpbGw9IiNGRkZGRkYiIGQ9Ik0wLDN2Mmg4VjNIMHoiLz4NCjwvc3ZnPg0K);
    box-shadow: none;
  }
}
</style>
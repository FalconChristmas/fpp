<template>
<div style="display: none">
  <div id="{{id}}" class="modal" v-bind:class="{ fade: fade, in: animateModal || !fade }" style="display: block" v-on:click="onClickOut($event)">
      <div class="modal-dialog  modal-{{size}}" role="document" style="z-index: 9999">
        <div class="modal-content-wrapper">
          <div class="modal-content">
              <div class="modal-header">
                <slot name="modal-header"></slot>
              </div>
              <div class="modal-body">
                <slot name="modal-body"></slot>
              </div>
              <div class="modal-footer">
                <slot name="modal-footer"></slot>
              </div>
          </div>
      </div>
    </div>
  </div>
  <div class="modal-backdrop" v-bind:class="{ fade: fade, in: animateBackdrop || !fade }"></div>
</div>
</template>
<script>
import {csstransitions} from '../../utils';

// this is directly linked to the bootstrap animation timing in _modal.scss
// // for browsers that do not support transitions like IE9 just change slide immediately
const TRANSITION_DURATION = csstransitions() ? 250 : 0

// export component object
export default {
  data() {
    return {
      animateBackdrop: false,
      animateModal: false,
    }
  },
  props: {
    id: {
      type: String,
      default: 'default'
    },
    size: {
      type: String,
      default: 'md'
    },
    fade: {
      type: Boolean,
      default: true
    },
    closeOnBackdrop: {
      type: Boolean,
      default: true,
    }
  },
  methods: {
    show() {
      this.$el.style.display = 'block'
      this._body = document.querySelector('body')
      const _this = this
      // wait for the display block, and then add class "in" class on the modal
      this._modalAnimation = setTimeout(() => {
        _this.animateBackdrop = true
        this._modalAnimation = setTimeout(() => {
          _this._body.classList.add('modal-open')
          _this.animateModal = true
          _this.$dispatch('shown::modal')
        }, (_this.fade) ? TRANSITION_DURATION : 0)
      }, 0)
    },
    hide() {
      const _this = this
      // first animate modal out
      this.animateModal = false
      this._modalAnimation = setTimeout(() => {
        // wait for animation to complete and then hide the backdrop
        _this.animateBackdrop = false
        this._modalAnimation = setTimeout(() => {
          _this._body.classList.remove('modal-open')
          // no hide the modal wrapper
          _this.$el.style.display = 'none'
          _this.$dispatch('hidden::modal')
        }, (_this.fade) ? TRANSITION_DURATION : 0)
      }, (_this.fade) ? TRANSITION_DURATION : 0)
    },
    onClickOut(e) {
      // if backdrop clicked, hide modal
      if (this.closeOnBackdrop && e.target.id && e.target.id === this.id) {
        this.hide()
      }
    },
  },
  events: {
    // control modal from outside via events
    'show::modal'(id) {
      if (id === this.id) {
        this.show()
      }
    },
    'hide::modal'(id) {
      if (id === this.id) {
        this.hide()
      }
    }
  },
  ready() {
    // support for esc key press
    document.addEventListener('keydown', (e) => {
      const key = e.which || e.keyCode
      if (key === 27) { // 27 is esc
        this.hide()
      }
    })
  },
  destroyed() {
    clearTimeout(this._modalAnimation)
  },
}
</script>
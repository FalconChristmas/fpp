<template>
   <div class="playlist-entry type-{{ entry.type }}">
        <div class="meta-row">
            <div class="entry-type">{{ entry.type == 'both' ? 'Media & Sequence' : entry.type }}</div>
        </div>
        <div class="main-row">
            <div v-if="entry.type == 'media' || entry.type == 'both'" class="media">
                <div class="title">{{entry.media}}</div>
                <div class="entry-label">Media</div>
            </div>
            <div v-if="entry.type == 'sequence' || entry.type == 'both'" class="sequence">
                <div class="title">{{entry.sequence}}</div>
                <div class="entry-label">Sequence</div>
            </div>
            <div v-if="entry.type == 'pause'" class="pause">
                <div class="title">{{entry.duration}} Seconds</div>
                <div class="entry-label">Pause</div>
            </div>
            <div v-if="entry.type == 'volume'" class="volume">
                <div class="title">{{entry.volume}}</div>
                <div class="entry-label">Set Volume</div>
            </div>
            <div v-if="entry.type == 'effect'" class="effect">
                <div class="title">{{entry.effect}}</div>
                <div class="entry-label">Effect</div>
            </div>
            <div v-if="entry.type == 'event'" class="major-id">
                <div class="title">{{entry.majorID}}</div>
                <div class="entry-label">Major ID</div>
            </div>
            <div v-if="entry.type == 'event'" class="minor-id">
                <div class="title">{{entry.minorID}}</div>
                <div class="entry-label">Minor ID</div>
            </div>
            <div v-if="entry.type == 'event'" class="blocking">
                <div class="title">{{entry.blocking ? 'Yes' : 'No'}}</div>
                <div class="entry-label">Blocking</div>
            </div>
            <div v-if="entry.type == 'pixelOverlay'" class="pixelOverlay">
                <div class="title">{{entry.sequence}}</div>
                <div class="entry-label">Sequence</div>
            </div>
            <div class="actions">
                <div class="edit-entry"></div>
                <div class="repeatable"><i class="ion-refresh" :class="{'text-danger' : entry.playOnce, 'text-success' : !entry.playOnce}"></i></div>
            </div>
        </div>
    </div>
</template>

<script>
import Popover from "../shared/popover.vue";

export default {
    components: {Popover},
    props: [ 'entry' ],
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
    .playlist-entry {
        background: black;
    }
</style>
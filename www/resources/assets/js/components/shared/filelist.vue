<template>
<div class="file-list table-responsive">
    <table class="table table-condensed table-hover">
        <thead>
            <th width="75%">Filename</th>
            <th>Last Modified</th>
        </thead>
        <tbody>
            <tr v-if="files" v-for="file in files">
                <td>{{ file.name }}</td>
                <td>{{ file.last_modified }}</td>
            </tr>
            <tr v-else>
                <td colspan="2" style="text-align: center;">No Files Found</td>
            </tr>
        </tbody>
    </table>
</div>
</template>
<script>
export default {
    components: {},
    props: ['files'],
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
    
    .file-list {
        .table th {
            border-top: 0;
        }
    }
    
</style>

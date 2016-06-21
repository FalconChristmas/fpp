<template>
<div class="event-list table-responsive">
    <table class="table table-sm">
        <thead>
            <th class="text-center">ID</th>
            <th>Name</th>
            <th>Script</th>
            <th>Effect</th>
            <th>Channel</th>
            <th>Actions</th>
        </thead>
        <tbody>
            <tr v-for="event in events">
                <td class="text-center">{{ event.id }}</td>
                <td>{{ event.name }}</td>
                <td>{{ event.script }}</td>
                <td>{{ event.effect }}</td>
                <td>{{ event.channel }}</td>
                <td>
                   <button class="table-action action-trigger hint--top" data-hint="Trigger Event" @click="handleAction('trigger')"><i class="ion-ios-play"></i></button>
                   <button class="table-action action-edit hint--top" data-hint="Edit Event" @click="handleAction('edit')"><i class="ion-edit"></i></button>
                   <button class="table-action action-delete hint--left" data-hint="Delete Event" @click="handleAction('delete')"><i class="ion-close"></i></button>
                </td>
            </tr>
            <tr v-if="!events" >
                <td colspan="6" style="text-align: center;">No Events Found</td>
            </tr>
        </tbody>
    </table>
</div>
</template>
<script>
export default {
    props: ['events'],
    data() {
        return {};
    },
    ready() {},
    methods: {
        handleAction(action) {

        }
    },
    events: {}

}
</script>

<style lang="sass">
@import "resources/assets/sass/_vars.scss";
@import "resources/assets/sass/_mixins.scss";

.event-list {
    .table {
        margin-top: 0;
        thead tr th:first-child {
                    padding-left: 0 !important;
        }
    }
    .table th {
        font-size: 10px;
        background: #f1f1f1;
        font-weight: normal;
        border-bottom: 0;
        border-top: 1px solid #e6e6e6;
        border-bottom: 1px solid #e6e6e6;
    }


}

</style>

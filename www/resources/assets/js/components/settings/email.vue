<template>
    <div id="email-settings" class="settings-container container-fluid box p-a-2">
        <h4><span class="semi-bold">Email</span> Settings</h4>     
        <hr>
        <div>
         <p>Outbound email requires a Gmail account to relay mail thru. Nothing is stored in/on your Gmail account. Gmail accounts are free by going to Gmail.com. By routing thru Gmail we go around when ISPs block outbound port 25 making you route email thru them.</p>
         <p>To send email from a script:</p>
         <ol>
             <li>Create your message to a temp file of your chosing</li>
             <li>Execute the mail utility: mail -s "This is the subject line" root@localhost < tempfile.txt</li>
             <li>If you need to attach a file also you can use the -a attachment_file param</li>
        </ol>
        <p>All outbound email via scripts needs to be sent to <strong>root@localhost</strong></p>
        </div>
        <form>
        </form>
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

</style>
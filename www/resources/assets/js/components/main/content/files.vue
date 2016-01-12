<template>
    <div class="file-manager">
        <section class="file-tabs">
            <div class="box">
                <Tabs classes="nav-tabs-simple">
                    <Tab header="Sequences">
                        <Filelist></Filelist>
                        <div class="info">
                                <p>Sequence files must be in the Falcon Pi Player .fseq format and may be converted from various other sequencer formats using xLights or Light-Elf. The Vixen 3 sequencer has the ability to directly export .fseq files.</p>
                        </div>
                    </Tab>
                    <Tab header="Audio">
                        <Filelist></Filelist>
                        <div class="info"><p>Audio files must be in MP3 or OGG format.</p></div>
                    </Tab>
                    <Tab header="Video">
                        <Filelist></Filelist>
                        <div class="info"><p>Video files must be in .mp4 or .mkv format. H264 video and AAC or MP3 audio are preferred because the video can be hardware accelerated on the Pi.</p></div>
                    </Tab>
                    <Tab header="Effects">
                        <Filelist></Filelist>
                        <div class="info"><p>Effects files are .fseq format files with an .eseq extension. These special sequence files contain only the channels for a specific effect and always start at channel 1 in the sequence file. The actual starting channel offset for the Effect is specified when you run it or configure the Effect in an Event.</p></div>
                    </Tab>
                    <Tab header="Scripts">
                        <Filelist></Filelist>
                        <div class="info"><p>Scripts must have a .sh, .pl, .pm, .php, or .py extension. Scripts may be executed inside an event. These might be used in a show to trigger an external action such as sending a message to a RDS capable FM transmitter or a non-DMX/Pixelnet LED sign.</p></div>
                    </Tab>
                    <Tab header="Logs">
                        <Filelist></Filelist>
                        <div class="info"><p>FPP logs may be viewed or downloaded for submission with bug reports.</p></div>
                    </Tab>
                    <Tab header="Uploads">
                        <Filelist></Filelist>
                        <div class="info"><p>The upload directory is used as temporary storage when uploading media and sequencee files. It is also used as permanent storage for other file formats which have no dedicated home.</p></div>
                    </Tab>
                </Tabs>
            </div>
        </section>
        <section class="upload">
            <div class="box">
                <div class="box-header">
                    <h4>Upload Files</h4>
                </div>
                <div class="box-body">
                   <form class="file-upload-form">
                       <div class="upload-message">
                         <h4>Drop files <span class="semi-bold">to Upload</span></h4>
                         <p class="text-muted">or click <span class="semi-bold">here</span></p>
                       </div>
                   </form>
                </div>
            </div>
        </section>
    </div>
</template>

<script>
import Tabs from "../../shared/Tabset.vue";
import Tab from "vue-strap/src/Tab.vue";
import Filelist from "../../shared/filelist.vue";

export default {
    components: { Tabs, Tab, Filelist },
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
    
    .file-manager {
        .nav-tabs {
            margin-bottom: 0 !important;
            ~ .tab-content {
                padding-top: 0;
            }
        }
        .tab-pane {
            .info {
                text-align: center;
                padding: 20px 20px 0;
                color: #788B94;
                border-top: 1px solid #eaeaea;
            }
        }

        .file-upload-form {
            min-height: 250px;
            padding: 25px;
            border: 1px dashed #eaeaea;
            border-radius: 2px;
            background: #fafafa;
            h4 {
                font-weight: 300;
                margin-bottom: 0;
            }
        }
        .upload-message {
            display: flex;
            flex: 1;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            height: 195px;
            p {
                margin: 0;
            }
        }
    }
</style>
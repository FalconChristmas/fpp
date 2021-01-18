import {MessageBus} from "assets/js/helpers/message-bus/message-bus";
import {renderClasses} from "assets/js/helpers/bem";
import axios from 'axios';
import tingle from "tingle.js";

const NAME = 'page';

const MESSAGE_HANDLERS = [
    'rebootpi.all',
    'shutdownPi.all',
    'setSetting.all',
];

class Page {

    constructor () {
        this.messageBus = new MessageBus();
        this.messageBus.connect(this);
    }

    onRebootpi () {
        this.showConformation({
            'content': 'Are you sure you want to reboot?',
            'callback': () => {
                axios({
                    method: 'GET',
                    url: `${FPP.url}/fppxml.php?command=rebootPi`,
                    headers: {
                        'Content-Type': 'text/xml',
                    }
                }).then((response) => { // Success callback
                    let modal = new tingle.modal({
                        closeMethods: [],
                    });
                    modal.setContent('<h1>FPP is rebooting</h1><p>Please wait until this screen disappears.<br />This normally is within 1 minute</p>');
                    modal.open();

                    // Set setting reboot to 0
                    this.messageBus.postMessage({
                        'message': 'setSetting',
                        'data': {
                            'setting': {
                                'key': 'rebootFlag',
                                'value': 0,
                            },
                        }
                    }, {
                        'name': this.componentName,
                        'scope': 'all',
                    });

                    setTimeout(() => {
                        setInterval(() => {
                            axios({
                                method: 'GET',
                                url: `${FPP.url}/fppjson.php?command=getFPPstatus`
                            }).then(response => {
                                if (response.status === 200) {
                                    location.reload();
                                }
                            });
                        }, 5000); // check if the device is turned on every 5 seconds
                    }, 10000); // wait 10 sec. until the polling begins
                }).catch(() => { // Error callback
                    this.messageBus.postMessage({
                        'message': 'revealNotification',
                        'data': {
                            'message': '<h1>Oops, an error</h1><p>FPP couldn\'t restart. Check if the device is turned on and has an ipaddress.</p>',
                        }
                    }, {
                        'name': this.componentName,
                        'scope': 'all',
                    });
                });
            }
        });
    }

    onShutdownpi () {
        this.showConformation({
            'content': 'Are you sure you want to shutdown?',
            'callback': () => {
                axios({
                    method: 'GET',
                    url: `${FPP.url}/fppjson.php?command=shutdownPi`
                }).then((response) => { // Success callback
                    this.messageBus.postMessage({
                        'message': 'revealErrorNotification',
                        'data': {
                            'message': '<h1>Turning off</h1><p>FPP has been shutdown, bye bye.</p>',
                        }
                    }, {
                        'name': this.componentName,
                        'scope': 'all',
                    });
                });
            }
        });
    }

    showConformation (params) {
        const {content, callback} = params;

        let modal = new tingle.modal({
            closeMethods: ['button', 'escape'],
            closeLabel: "close",
            footer: true,
        });

        // set content
        modal.setContent(content);

        // Add continue button
        modal.addFooterBtn('Yes', `tingle-btn ${renderClasses('tingle-btn', null, ['primary', 'pull-right'])}`, () => {
            callback();
            modal.close();
        });

        // Add cancel button
        modal.addFooterBtn('cancel', `tingle-btn ${renderClasses('tingle-btn', null, ['pull-right'])}`, () => {
            modal.close();
        });

        modal.open();
    }

    onSetSetting (evt) {
        axios({
            method: 'GET',
            url: `${FPP.url}/fppjson.php?command=setSetting&key="${evt.data.data.setting.key}"&value="${encodeURIComponent(evt.data.data.setting.value)}`
        }).then((response) => { // Success callback
            this.messageBus.postMessage({
                'message': 'revealToastify',
                'data': {
                    'message': `Setting ${evt.data.data.setting.key} saved`,
                }
            }, {
                'name': this.componentName,
                'scope': 'all',
            });
        });
    }

    get componentName () {
        return NAME;
    }

    get messageHandlers () {
        return MESSAGE_HANDLERS;
    }
}

export default Page;

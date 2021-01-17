import {ComponentController} from 'assets/js/helpers/stimulus/component-controller';
import {renderClasses} from "assets/js/helpers/bem";
import axios from 'axios';
import tingle from "tingle.js";

const NAME = 'sidenav';

const CLASSES = {
    'toggleButton': renderClasses(NAME, 'toggle'),
    'toggleButtonClose': renderClasses(NAME, 'toggle', ['close']),
    'pageSidenavContainer': renderClasses('page', 'sidenav'),
    'pageSidenavContainerOpen': renderClasses('page', 'sidenav', ['open']),
};

const SELECTORS = {
    'toggleButton': `.${CLASSES.toggleButton}`,
    'pageSidenavContainer': `.${CLASSES.pageSidenavContainer}`,
};

class Sidenav extends ComponentController {
    toggleMenu (evt) {
        evt.preventDefault();
        evt.stopPropagation();

        let toggleButton = document.querySelector(SELECTORS.toggleButton);
        let pageSidenavContainer = document.querySelector(SELECTORS.pageSidenavContainer);

        if (!toggleButton.classList.contains(CLASSES.toggleButtonClose)) {
            pageSidenavContainer.classList.add(CLASSES.pageSidenavContainerOpen);
            toggleButton.classList.add(CLASSES.toggleButtonClose);
        } else {
            pageSidenavContainer.classList.remove(CLASSES.pageSidenavContainerOpen);
            toggleButton.classList.remove(CLASSES.toggleButtonClose);
        }
    }

    reboot (evt) {
        evt.preventDefault();
        evt.stopPropagation();

        this.showConformation({
            'content': 'Are you sure you want to reboot?',
            'callback': () => {
                axios({
                    method: 'GET',
                    url: `${FPP.url}/fppjson.php?command=setSetting&key=restartFlag&value=0`
                }).then((response) => { // Success callback
                    let modal = new tingle.modal({
                        closeMethods: [],
                    });
                    modal.setContent('<h1>FPP is rebooting</h1><p>Please wait until this screen disappears.<br />This normally is within 1 minute</p>');
                    modal.open();

                    setTimeout(() => {
                        let statusPolling = setInterval(() => {
                            axios({
                                method: 'GET',
                                url: `${FPP.url}/fppjson.php?command=getFPPstatus`
                            }).then(response => {
                                if (response.status === 200) {
                                    statusPolling.clearInterval();
                                    location.reload();
                                }
                            });
                        }, 2000);
                    }, 10000); // wait 10 sec. until the polling begins
                }).catch(() => { // Error callback
                    this.messageBus.postMessage({
                        'message': 'revealNotification',
                        'data': {
                            'message': '<h1>Oops, an error</h1><p>FPP couldn\'t restart. Check if the device is turned on and has an ipaddress.</p>',
                        }
                    });
                });
            }
        });

    }

    shutdown (evt) {
        evt.stopPropagation();
        evt.preventDefault();

        this.showConformation({
            'content': 'Are you sure you want to shutdown?',
            'callback': () => {
                axios({
                    method: 'GET',
                    url: `${FPP.url}/fppjson.php?command=setSetting&key=restartFlag&value=0`
                }).then((response) => { // Success callback
                    this.messageBus.postMessage({
                        'message': 'revealErrorNotification',
                        'data': {
                            'message': '<h1>Turning off</h1><p>FPP has been shutdown, bye bye.</p>',
                        }
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
}

export default {
    'name': NAME,
    'controller': Sidenav
};

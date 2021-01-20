import {ComponentController} from 'assets/js/helpers/stimulus/component-controller';
import axios from "axios";
import {wire, hyper} from 'hyperhtml/umd';

const NAME = 'deviceinformation';

class Deviceinformation extends ComponentController {
    static targets = ['deviceinformationRules'];

    connect () {
        this.buildDeviceInfoTable('');
        // const pollDeviceInformation = setInterval(() => {
        //     this.pollDeviceInformation();
        // }, 1000);
    }

    async pollDeviceInformation () {
        const response = await axios({
            method: 'GET',
            url: 'http://192.168.178.19/fppjson.php?command=getFPPstatus'
        });

        // Check if the response code is 200, else we need to return something else
        if (response.status === 200) {
            const wiredElement = wire()`${[response.data]}`;
            hyper(this.element)`${wiredElement.childNodes[1]}`;
        } else {
            this.messageBus.postMessage({
                'message': 'revealErrorToastify',
                'data': {
                    'message': 'Error getting device information',
                }
            });
        }
    }

    buildDeviceInfoTable(deviceInfo) {
        console.log(wire()`<table></table>`);
    }
}

export default {
    'name': NAME,
    'controller': Deviceinformation
};

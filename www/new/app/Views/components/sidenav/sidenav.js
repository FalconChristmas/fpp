import {ComponentController} from 'assets/js/helpers/stimulus/component-controller';

const NAME = 'sidenav';

class Sidenav extends ComponentController {
    connect () {
        console.log('Hello World!');
    }
}

export default {
    'name': NAME,
    'controller': Sidenav
};

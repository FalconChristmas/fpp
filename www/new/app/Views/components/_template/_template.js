import {ComponentController} from 'assets/js/helpers/stimulus/component-controller';

const NAME = '%name%';

class %name_camelcase% extends ComponentController {
    connect () {
        console.log('Hello World!');
    }
}

export default {
    'name': NAME,
    'controller': %name_camelcase%
};

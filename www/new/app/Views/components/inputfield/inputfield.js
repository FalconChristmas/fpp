import {ComponentController} from 'assets/js/helpers/stimulus/component-controller';

const NAME = 'inputfield';

class Inputfield extends ComponentController {

    connect () {
        super.connect();
    }

    bgColors = ['red', 'blue', 'yellow', 'green', 'purple', 'pink'];

    changeBackgroundColor () {
        document.body.style.backgroundColor = this.bgColors[Math.floor(Math.random() * 7)];
    }
}

export default {
    'name': NAME,
    'controller': Inputfield
};

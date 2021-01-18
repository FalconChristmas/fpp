import {MessageBus} from "assets/js/helpers/message-bus/message-bus";
import Toastify from 'toastify-js';

const NAME = 'toastifymodal';

const MESSAGE_HANDLERS = [
    'revealToastify.all',
    'revealErrorToastify.all',
];

class ToastifyModal {

    constructor () {
        this.messageBus = new MessageBus();
        this.messageBus.connect(this);
    }

    onRevealToastify (evt) {
        let type = renderClasses('toastify', null, ['success']);

        switch (evt.data.data.type) {
            case 'info':
                type = renderClasses('toastify', null, ['info']);
                break;
            case 'error':
                type = renderClasses('toastify', null, ['error']);
                break;
            case 'success':
        }

        let toastify = Toastify({
            text: evt.data.data.message,
            duration: 5000,
            close: true,
            stopOnFocus: true,
            gravity: "top",
            position: "right",
            className: type,
        });

        toastify.showToast();
    }

    get componentName () {
        return NAME;
    }

    get messageHandlers () {
        return MESSAGE_HANDLERS;
    }
}

export default ToastifyModal;

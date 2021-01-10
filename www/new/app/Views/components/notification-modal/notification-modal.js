import tingle from "tingle.js";
import {MessageBus} from "../../../assets/js/helpers/message-bus/message-bus";

const NAME = 'notification-modal';

const MESSAGE_HANDLERS = [
    'revealNotification.all',
    'closeNotification.all',
];

class NotificationModal {

    constructor () {
        this.messageBus = new MessageBus();
        this.messageBus.connect(this);
        this.modal = new tingle.modal({
            closeMethods: ['overlay', 'button', 'escape'],
            closeLabel: "sluiten",
        });
    }

    onRevealNotification (evt) {
        this.modal.setContent(evt.data.data.message);
        this.modal.open();
    }

    get componentName () {
        return NAME;
    }

    get messageHandlers () {
        return MESSAGE_HANDLERS;
    }
}

export default NotificationModal;

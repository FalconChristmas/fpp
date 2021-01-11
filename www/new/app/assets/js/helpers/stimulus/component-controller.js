import {Controller} from 'stimulus';

import {MessageBus} from '../../helpers/message-bus/message-bus';

const NAME = 'component';
const MESSAGE_HANDLERS = [];

const messageBus = new MessageBus();

class ComponentController extends Controller {
    initialize () {
        this.messagesScope = this.data.get('scope') || 'all';

        this.messageBus = {
            postMessage: (data) => {
                messageBus.postMessage(data, {
                    'name': this.componentName,
                    'scope': this.messagesScope,
                })
            }
        };
    }

    connect () {
        messageBus.connect(this);
    }

    disconnect () {
        messageBus.disconnect(this);
    }

    get componentName () {
        return NAME;
    }

    get messageHandlers () {
        return MESSAGE_HANDLERS;
    }
}

export {ComponentController};

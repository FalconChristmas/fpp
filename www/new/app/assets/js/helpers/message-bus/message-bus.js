import {origin} from '../env/origin';

const ALLOWED_ORIGINS = [
    origin,
];

class MessageBus {
    constructor () {
        this.messageMapping = {};

        window.addEventListener('message', this.onMessage.bind(this), false);
    }

    onMessage (evt) {
        this.handleMessage(evt);
    }

    connect (controller) {
        const messageHandlers = controller.messageHandlers;

        messageHandlers.forEach(messageName => {
            if (!this.messageMapping[messageName]) {
                this.messageMapping[messageName] = [];
            }

            this.messageMapping[messageName].push(controller)
        });
    }

    disconnect (controller) {
        const messageHandlers = controller.messageHandlers;

        messageHandlers.forEach(messageName => {
            if (!this.messageMapping[messageName]) {
                return;
            }

            const index = this.messageMapping[messageName].indexOf(controller);
            this.messageMapping[messageName].splice(index, 1);
        });
    }

    handleMessage (evt) {
        const isFromCorrectOrigin = ALLOWED_ORIGINS.indexOf(evt.origin) != -1;

        if (!isFromCorrectOrigin) {
            return;
        }

        const scopedMessage = evt.data.message;

        if (!scopedMessage) {
            return;
        }

        const messages = [scopedMessage];

        const splitMessage = scopedMessage.split('.');
        const scope = splitMessage[splitMessage.length - 1];

        if (scope != 'all') {
            splitMessage[splitMessage.length - 1] = 'all';

            const genericMessage = splitMessage.join('.');

            messages.push(genericMessage);
        }

        messages.forEach(message => {
            const handlingControllers = this.messageMapping[message] || [];
            const handlerMethodName = this.getHandlerMethodName(message);

            handlingControllers.forEach(controller => {
                if (handlerMethodName in controller) {
                    controller[handlerMethodName](evt);
                    this.handleMessageLogger(message, controller, handlerMethodName, evt.data);
                } else {
                    console.error(`Handler "${handlerMethodName}" not found on "${controller.componentName}"`);
                }
            });
        });
    }

    postMessage (messageData, sender) {
        const data = messageData.data;
        const message = `${messageData.message}.${sender.scope}`;

        const postMessageData = {
            'message': message,
            'data': data,
        };

        this.postMessageLogger(message, sender, postMessageData);

        window.postMessage(postMessageData, origin);
    }

    handleMessageLogger (message, controller, handler, data) {
        if (!window.messageLogger) {
            return;
        }

        console.info(`HANDLE: "${message}" handled by "${controller.componentName}" calling "${handler}"`, data);
    }

    postMessageLogger (message, sender, postMessageData) {
        if (!window.messageLogger) {
            return;
        }

        console.info(`POST: "${message}" posted by "${sender.name}"`, postMessageData);
    }

    getHandlerMethodName (message) {
        // transform 'componentNameEvent.scope' to 'onComponentNameEvent.scope';
        message = `on${message.replace(/^\w/, c => c.toUpperCase())}`;
        // strip '.scope';
        message = message.split('.')[0];

        return message;
    }
}

export {MessageBus};

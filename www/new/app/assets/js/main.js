// src/main.js
import {Application} from 'stimulus';
import App from '../../assets/js/app';
import {ComponentController} from "./helpers/stimulus/component-controller";
import Inputfield from 'Views/components/inputfield/inputfield';
/* component generator replace here */

new ComponentController();
const controllers = [
    Inputfield,
    /* js component here */
];

const application = Application.start();

controllers.forEach((controllerModule) => {
    application.register(controllerModule.name, controllerModule.controller);
});

// this is the main application entry point
new App();

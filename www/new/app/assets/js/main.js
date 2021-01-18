// src/main.js
import {Application} from 'stimulus';
import App from '../../assets/js/app';
import {ComponentController} from "./helpers/stimulus/component-controller";
import Deviceinformation from 'Views/components/deviceinformation/deviceinformation';
import Sidenav from 'Views/components/sidenav/sidenav';
import Page from 'Views/components/page/page';
/* component generator replace here */

new ComponentController();
const controllers = [
    Deviceinformation,
    Sidenav,
    /* js component here */
];

const application = Application.start();

controllers.forEach((controllerModule) => {
    application.register(controllerModule.name, controllerModule.controller);
});

// this is the main application entry point
new App();

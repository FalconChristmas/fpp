import {ComponentController} from 'assets/js/helpers/stimulus/component-controller';
import {renderClasses} from "assets/js/helpers/bem";

const NAME = 'sidenav';

const CLASSES = {
    'toggleButton': renderClasses(NAME, 'toggle'),
    'toggleButtonClose': renderClasses(NAME, 'toggle', ['close']),
    'pageSidenavContainer': renderClasses('page', 'sidenav'),
    'pageSidenavContainerOpen': renderClasses('page', 'sidenav', ['open']),
};

const SELECTORS = {
    'toggleButton': `.${CLASSES.toggleButton}`,
    'pageSidenavContainer': `.${CLASSES.pageSidenavContainer}`,
};

class Sidenav extends ComponentController {
    toggleMenu (evt) {
        evt.preventDefault();
        evt.stopPropagation();

        let toggleButton = document.querySelector(SELECTORS.toggleButton);
        let pageSidenavContainer = document.querySelector(SELECTORS.pageSidenavContainer);

        if (!toggleButton.classList.contains(CLASSES.toggleButtonClose)) {
            pageSidenavContainer.classList.add(CLASSES.pageSidenavContainerOpen);
            toggleButton.classList.add(CLASSES.toggleButtonClose);
        } else {
            pageSidenavContainer.classList.remove(CLASSES.pageSidenavContainerOpen);
            toggleButton.classList.remove(CLASSES.toggleButtonClose);
        }
    }

    reboot (evt) {
        evt.preventDefault();

        this.messageBus.postMessage({
            'message': 'rebootPi',
        });
    }

    shutdown (evt) {
        evt.preventDefault();

        this.messageBus.postMessage({
            'message': 'shutdownPi',
        });
    }
}

export default {
    'name': NAME,
    'controller': Sidenav
};

import NotificationModal from "Views/components/notification-modal/notification-modal";

const App = (() => {
    class App {
        constructor () {
            window.app = this;
            window.messageLogger = false;
            new NotificationModal();
        }
    }

    return App;
})();

export default App;

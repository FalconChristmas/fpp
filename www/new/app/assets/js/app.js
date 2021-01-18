import NotificationModal from "Views/components/notification-modal/notification-modal";
import Page from "Views/components/page/page";
import ToastifyModal from "Views/components/toastifymodal/toastifymodal";

const App = (() => {
    class App {
        constructor () {
            window.app = this;
            window.messageLogger = false;
            new NotificationModal();
            new ToastifyModal();
            new Page(); // default application javascript
        }
    }

    return App;
})();

export default App;

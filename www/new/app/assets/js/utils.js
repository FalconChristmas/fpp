export const Utils = {
    isMobile: (includeTablet = true) => {
        if (!includeTablet) {
            return window.innerWidth < 768
        }

        return window.innerWidth < 992
    },
    isDev: () => {
        return document.getElementsByClassName('page--dev').length > 0;
    }
};

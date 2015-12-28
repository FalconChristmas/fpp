/**
 * Responsible for all HTTP requests.
 *
 */

export default {
    request(method, url, data, cb = null, options = {}) {
        options = Object.assign(options, {
            error: (data, status, request) => {
                if (status === 401) {
                    document.location.href = "/login";
                }
            },
        });

        switch (method) {
            case 'get':
                return Vue.http.get(url, data, cb, options);
            case 'post':
                return Vue.http.post(url, data, cb, options);
            case 'put':
                return Vue.http.put(url, data, cb, options);
            case 'delete':
                return Vue.http.delete(url, data, cb, options);
            default:
                break;
        }
    },

    get(url, data = {}, cb = null, options = {}) {
        return this.request('get', url, data, cb, options);
    },

    post(url, data, cb = null, options = {}) {
        return this.request('post', url, data, cb, options);
    },

    put(url, data, cb = null, options = {}) {
        return this.request('put', url, data, cb, options);
    },

    delete(url, data = {}, cb = null, options = {}) {
        return this.request('delete', url, data, cb, options);
    },

    /**
     * A shortcut method to ping and check if the user session is still valid.
     */
    ping() {
        return this.get('/');
    },
};

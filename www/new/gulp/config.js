/* Configuration */
const resourcesPath = './app';
const publicPath = './public';
const assetsPath = `${resourcesPath}/assets`;
const vendorPath = './node_modules';

const config = {
    stylesSrcPath: `${assetsPath}/styles`,
    stylesDstPath: `${publicPath}/css/`,
    jsDstPath: `${publicPath}/js/`,
    componentsDstPath: `${resourcesPath}/Views/components/`,
};

module.exports = {
    resourcesPath,
    publicPath,
    assetsPath,
    vendorPath,
    config,
};

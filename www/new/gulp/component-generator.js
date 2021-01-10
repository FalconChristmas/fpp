const {config, resourcesPath} = require('./config');
const argv = require('yargs').argv;
const fs = require('fs');
const locater = require('locater');
const plugins = require('gulp-load-plugins')();

function ucFirst (string) {
    return string.charAt(0).toUpperCase() + string.slice(1);
}

function camelCased (string) {
    return ucFirst(string.replace(/-([A-z])/g, (g) => {
        return g[1].toUpperCase();
    }));
}

function snakeCased (string) {
    return string.replace(/-([A-z])/g, (g) => {
        return `_${g[1]}`.toLowerCase();
    });
}

function kebabCased (string) {
    return string.replace(/-([A-z])/g, (g) => {
        return `-${g[1]}`.toLowerCase();
    });
}

module.exports = function generate (cb) {
    const componentPath = `${config.componentsDstPath}/${argv.name}`;

    /* Check and create folder */
    if (!fs.existsSync(componentPath)) {
        fs.mkdirSync(componentPath);
        plugins.util.log('Finished', plugins.util.colors.green("-- Folder created"));
    }
    /* Check and create folder */

    /* Check and create JS file */
    if (!argv.nojs) {
        if (!fs.existsSync(`${componentPath}/${argv.name}.js`)) {
            const camelCasedName = camelCased(argv.name);

            fs.readFile(`${config.componentsDstPath}_template/_template.js`, 'utf8', (err, data) => {
                if (err) {
                    return console.log(err);
                }
                const result = data.replace(/%name%/g, argv.name)
                    .replace(/%name_camelcase%/g, camelCasedName);

                fs.writeFileSync(`${componentPath}/${argv.name}.js`, result);
            });
            plugins.util.log('Finished', plugins.util.colors.green("-- JS File created"));

            let insertLocation = `${resourcesPath}/assets/js/main.js`;
            let filename = `import ${camelCasedName} from 'Views/components/${argv.name}/${argv.name}';`;

            insertIntoFile({
                replacementKey: '/* js component here */',
                string: `    ${camelCasedName},`,
                fileWithPath: insertLocation
            });

            insertIntoFile({
                string: filename,
                fileWithPath: insertLocation
            });
        }
    }
    /* Check and create JS file */

    /* Check and create SCSS file */
    if (!argv.noscss) {
        if (!fs.existsSync(`${componentPath}/${argv.name}.scss`)) {
            fs.readFile(`${config.componentsDstPath}_template/_template.scss`, 'utf8', (err, data) => {
                if (err) {
                    return console.log(err);
                }

                const result = data.replace(/%name%/g, argv.name);

                fs.writeFileSync(`${componentPath}/${argv.name}.scss`, result);
            });

            plugins.util.log('Finished', plugins.util.colors.green("-- SCSS File created"));

            let insertLocation = `${resourcesPath}/assets/styles/_components.scss`;

            insertIntoFile({
                string: `@import '../../Views/components/${argv.name}/${argv.name}';`,
                fileWithPath: insertLocation
            });
        }
    }
    /* Check and create SCSS file */

    /* Check and create Twig file */
    if (!argv.notwig) {
        if (!fs.existsSync(`${componentPath}/${argv.name}.html.twig`)) {
            fs.readFile(`${config.componentsDstPath}_template/_template.html.twig`, 'utf8', (err, data) => {
                if (err) {
                    return console.log(err);
                }

                const result = data
                    .replace(/%name%/g, argv.name)
                    .replace(/%name_snake_cased%/g, snakeCased(argv.name))
                    .replace(/%name_kebab_cased%/g, kebabCased(argv.name))
                    .replace(/%jscontroller%/, (argv.nojs ? '' : '\'data-controller\': name,'));

                fs.writeFileSync(`${componentPath}/${argv.name}.html.twig`, result);
            });

            plugins.util.log('Finished', plugins.util.colors.green("-- Twig File created"));
        }
    }
    /* Check and create Twig file */

    cb();
}

const insertIntoFile = ({replacementKey = '/* component generator replace here */', string, fileWithPath}) => {
    let inputFile = fs.readFileSync(fileWithPath, {encoding: 'utf8'});
    let file = locater.findOne(replacementKey, inputFile);

    const data = inputFile.split("\n");
    data.splice((file.line === 0 ? 0 : file.line - 1), 0, string);
    fs.writeFileSync(fileWithPath, data.join("\n"));
};

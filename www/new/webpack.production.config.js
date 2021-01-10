const path = require('path');

module.exports = {
    mode: 'production',
    module: {
        rules: [
            {
                test: /\.js$/,
                exclude: [/node_modules/, /app\/Views\/components\/_template/], // This may not be needed since we supplied `include`.
                include: [`${__dirname}/src`, `${__dirname}/app/Views/components`, `${__dirname}/assets/js`],
                use: [
                    {
                        loader: 'babel-loader',
                        options: {
                            presets: [
                                [
                                    '@babel/preset-env',
                                    {
                                        targets: {
                                            browsers: [
                                                "last 2 major versions",
                                                "> 1%",
                                                "IE 11"
                                            ]
                                        },
                                        modules: false
                                    }
                                ],
                            ],
                            // List of Babel plugins.
                            plugins: [
                                '@babel/plugin-proposal-object-rest-spread',
                                '@babel/plugin-proposal-class-properties',
                                '@babel/plugin-transform-classes'
                            ]
                        }
                    }
                ]
            }
        ]
    },
    resolve: {
        modules: [ path.resolve(__dirname, './app'), 'node_modules' ],
        extensions: [ '.js', ],
    },
    plugins: [],
};

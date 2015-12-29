var lost = require('lost')
module.exports = {
  sass: {
    includePaths: ['node_modules/breakpoint-sass/stylesheets/']
  },
  // postcss plugins
  postcss: [lost()],
  // configure autoprefixer
  autoprefixer: {
    browsers: ['last 2 versions']
  },
 
}
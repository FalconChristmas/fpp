var lost = require('lost')
module.exports = {
  // postcss plugins
  postcss: [lost()],
  // configure autoprefixer
  autoprefixer: {
    browsers: ['last 2 versions']
  },
 
}
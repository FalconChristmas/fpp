=====
New UI Developer Notes
=====

There are two main stylesheets used by the new UI:
- /css/fpp.css (used primarily for page layouts and individual styling)
- /css/fpp-bootstrap/dist/fpp-bootstrap.min.css  (used primarily overall theming, utilties and components)

The latter, fpp-bootstrap, new UI makes the SCSS-based bootstrap framework for grid, buttons, utilties, variabled & mixins.
This file is compiled from SCSS during development using gulp.
It is not necessary to compile this unless you are a developer and wish to make changes to the style framework.

How to compile:
1. Have node.js installed (on the computer you are developing on)
2. From terminal/command line (make sure your are in www directory): `npm install && npm install gulp-cli -g`
3. gulp watch-bs
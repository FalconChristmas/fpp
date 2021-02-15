=====
New UI Developer Notes
=====

There are two main stylesheets used by the new UI:
- /css/fpp.css (used primarily for page layouts and individual styling)
- /css/fpp-bootstrap/dist/fpp-bootstrap.min.css  (used primarily overall theming, utilties and components)

The latter, fpp-bootstrap, new UI makes the SCSS-based bootstrap framework for grid, buttons, utilties, variabled & mixins.
This file is compiled from SCSS during development using gulp.
It is not necessary to compile this unless you are a developer and wish to make changes to the style framework.

How to compile scss on the Pi from command line (optional):
```sudo su
curl -o- https://raw.githubusercontent.com/creationix/nvm/v0.33.11/install.sh | bash
nvm install node
npm install gulp-cli -g
cd /opt/fpp/www
npm install
gulp```
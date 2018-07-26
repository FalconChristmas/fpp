'use strict';

module.exports = function(grunt) {

  // Project configuration.
  grunt.initConfig({
    // Metadata.
    pkg: grunt.file.readJSON('smartmenus.jquery.json'),

    banner: '/*!\n\
 * <%= pkg.title || pkg.name %> jQuery Plugin - v<%= pkg.version %> - <%= grunt.template.today("mmmm d, yyyy") %>\n\
 * <%= pkg.homepage %>\n\
 *\n\
 * Copyright <%= pkg.author.name %>, Vadikom Web Ltd.\n\
 * <%= pkg.author.url %>\n\
 *\n\
 * Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %>\n\
 */\n\n',

    banner_min: '/*! <%= pkg.title || pkg.name %> jQuery Plugin - v<%= pkg.version %> - <%= grunt.template.today("mmmm d, yyyy") %>\n\
 * <%= pkg.homepage %>\n\
 * Copyright <%= pkg.author.name %>, Vadikom Web Ltd. <%= pkg.author.url %>; Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %> */',

    banner_keyboard_addon: '/*!\n\
 * <%= pkg.title || pkg.name %> jQuery Plugin Keyboard Addon - v<%= pkg.version_keyboard_addon %> - <%= grunt.template.today("mmmm d, yyyy") %>\n\
 * <%= pkg.homepage %>\n\
 *\n\
 * Copyright <%= pkg.author.name %>, Vadikom Web Ltd.\n\
 * <%= pkg.author.url %>\n\
 *\n\
 * Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %>\n\
 */\n\n',

    banner_keyboard_addon_min: '/*! <%= pkg.title || pkg.name %> jQuery Plugin Keyboard Addon - v<%= pkg.version_keyboard_addon %> - <%= grunt.template.today("mmmm d, yyyy") %>\n\
 * <%= pkg.homepage %>\n\
 * Copyright <%= pkg.author.name %>, Vadikom Web Ltd. <%= pkg.author.url %>; Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %> */',

    banner_bootstrap_addon: '/*!\n\
 * <%= pkg.title || pkg.name %> jQuery Plugin Bootstrap Addon - v<%= pkg.version_bootstrap_addon %> - <%= grunt.template.today("mmmm d, yyyy") %>\n\
 * <%= pkg.homepage %>\n\
 *\n\
 * Copyright <%= pkg.author.name %>, Vadikom Web Ltd.\n\
 * <%= pkg.author.url %>\n\
 *\n\
 * Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %>\n\
 */\n\n',

    banner_bootstrap_addon_min: '/*! <%= pkg.title || pkg.name %> jQuery Plugin Bootstrap Addon - v<%= pkg.version_bootstrap_addon %> - <%= grunt.template.today("mmmm d, yyyy") %>\n\
 * <%= pkg.homepage %>\n\
 * Copyright <%= pkg.author.name %>, Vadikom Web Ltd. <%= pkg.author.url %>; Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %> */',

    banner_bootstrap_4_addon: '/*!\n\
 * <%= pkg.title || pkg.name %> jQuery Plugin Bootstrap 4 Addon - v<%= pkg.version_bootstrap_4_addon %> - <%= grunt.template.today("mmmm d, yyyy") %>\n\
 * <%= pkg.homepage %>\n\
 *\n\
 * Copyright <%= pkg.author.name %>, Vadikom Web Ltd.\n\
 * <%= pkg.author.url %>\n\
 *\n\
 * Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %>\n\
 */\n\n',

    banner_bootstrap_4_addon_min: '/*! <%= pkg.title || pkg.name %> jQuery Plugin Bootstrap 4 Addon - v<%= pkg.version_bootstrap_4_addon %> - <%= grunt.template.today("mmmm d, yyyy") %>\n\
 * <%= pkg.homepage %>\n\
 * Copyright <%= pkg.author.name %>, Vadikom Web Ltd. <%= pkg.author.url %>; Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %> */',

    distdir: '<%= pkg.name %>-<%= pkg.version %>',

    // Task configuration.
    clean: ['zip/*', 'dist/*'],
    concat: {
      dist: {
        options: {
          banner: '<%= banner %>',
          stripBanners: true
        },
        src: ['src/jquery.<%= pkg.name %>.js'],
        dest: 'zip/<%= distdir %>/jquery.<%= pkg.name %>.js'
      },
      dist_keyboard_addon: {
        options: {
          banner: '<%= banner_keyboard_addon %>',
          stripBanners: true
        },
        src: ['src/addons/keyboard/jquery.<%= pkg.name %>.keyboard.js'],
        dest: 'zip/<%= distdir %>/addons/keyboard/jquery.<%= pkg.name %>.keyboard.js'
      },
      dist_bootstrap_addon: {
        options: {
          banner: '<%= banner_bootstrap_addon %>',
          stripBanners: true
        },
        src: ['src/addons/bootstrap/jquery.<%= pkg.name %>.bootstrap.js'],
        dest: 'zip/<%= distdir %>/addons/bootstrap/jquery.<%= pkg.name %>.bootstrap.js'
      },
      dist_bootstrap_4_addon: {
        options: {
          banner: '<%= banner_bootstrap_4_addon %>',
          stripBanners: true
        },
        src: ['src/addons/bootstrap-4/jquery.<%= pkg.name %>.bootstrap-4.js'],
        dest: 'zip/<%= distdir %>/addons/bootstrap-4/jquery.<%= pkg.name %>.bootstrap-4.js'
      }
    },
    uglify: {
      dist: {
        options: {
          banner: '<%= banner_min %>'
        },
        src: 'zip/<%= distdir %>/jquery.<%= pkg.name %>.js',
        dest: 'zip/<%= distdir %>/jquery.<%= pkg.name %>.min.js'
      },
      dist_keyboard_addon: {
        options: {
          banner: '<%= banner_keyboard_addon_min %>'
        },
        src: 'zip/<%= distdir %>/addons/keyboard/jquery.<%= pkg.name %>.keyboard.js',
        dest: 'zip/<%= distdir %>/addons/keyboard/jquery.<%= pkg.name %>.keyboard.min.js'
      },
      dist_bootstrap_addon: {
        options: {
          banner: '<%= banner_bootstrap_addon_min %>'
        },
        src: 'zip/<%= distdir %>/addons/bootstrap/jquery.<%= pkg.name %>.bootstrap.js',
        dest: 'zip/<%= distdir %>/addons/bootstrap/jquery.<%= pkg.name %>.bootstrap.min.js'
      },
      dist_bootstrap_4_addon: {
        options: {
          banner: '<%= banner_bootstrap_4_addon_min %>'
        },
        src: 'zip/<%= distdir %>/addons/bootstrap-4/jquery.<%= pkg.name %>.bootstrap-4.js',
        dest: 'zip/<%= distdir %>/addons/bootstrap-4/jquery.<%= pkg.name %>.bootstrap-4.min.js'
      }
    },
    sass: {
      dist: {
        options: {
          style: 'expanded',
        },
        files: [{
          expand: true,
          cwd: 'src/css/',
          src: ['**/*.scss'],
          dest: 'src/css/',
          ext: '.css'
        }]
      }
    },
    copy: {
      main: {
        files: [
          // copy stuff to zip/
          { src: ['LICENSE-MIT', 'README.md'], dest: 'zip/<%= distdir %>/' },
          { src: ['src/addons/bootstrap/jquery.smartmenus.bootstrap.css'], dest: 'zip/<%= distdir %>/addons/bootstrap/jquery.smartmenus.bootstrap.css' },
          { src: ['src/addons/bootstrap-4/jquery.smartmenus.bootstrap-4.css'], dest: 'zip/<%= distdir %>/addons/bootstrap-4/jquery.smartmenus.bootstrap-4.css' },
          { expand: true, cwd: 'src/', src: ['css/**', 'demo/**', 'libs/**'], dest: 'zip/<%= distdir %>/' },
          // copy stuff to dist/
          { src: ['zip/<%= distdir %>/jquery.smartmenus.js'], dest: 'dist/jquery.smartmenus.js' },
          { src: ['zip/<%= distdir %>/jquery.smartmenus.min.js'], dest: 'dist/jquery.smartmenus.min.js' },
          { src: ['src/addons/bootstrap/jquery.smartmenus.bootstrap.css'], dest: 'dist/addons/bootstrap/jquery.smartmenus.bootstrap.css' },
          { src: ['src/addons/bootstrap-4/jquery.smartmenus.bootstrap-4.css'], dest: 'dist/addons/bootstrap-4/jquery.smartmenus.bootstrap-4.css' },
          { expand: true, cwd: 'src/', src: ['css/**'], dest: 'dist/' },
          { expand: true, cwd: 'zip/<%= distdir %>/', src: ['addons/**'], dest: 'dist/' }
        ]
      }
    },
    zip: {
      dist: {
        router: function (filepath) {
          // remove "zip/" from filepath
          return filepath.replace(/^zip\//, '');
        },
        src: ['zip/<%= distdir %>/**'],
        dest: 'zip/<%= distdir %>.zip'
      }
    },
    watch: {
      scss: {
        files: 'src/css/**/*.scss',
        tasks: ['sass']
      }
    }
  });

  // These plugins provide necessary tasks.
  grunt.loadNpmTasks('grunt-contrib-clean');
  grunt.loadNpmTasks('grunt-contrib-concat');
  grunt.loadNpmTasks('grunt-contrib-uglify');
  grunt.loadNpmTasks('grunt-contrib-sass');
  grunt.loadNpmTasks('grunt-contrib-copy');
  grunt.loadNpmTasks('grunt-zip');
  grunt.loadNpmTasks('grunt-contrib-watch');

  // Default task.
  grunt.registerTask('default', ['clean', 'concat', 'uglify', 'sass', 'copy', 'zip']);

};

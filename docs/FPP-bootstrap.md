## Compiling fpp-bootstrap.css from bootstrap scss framework

The FPP application runs using a customised version of bootstrap
This customised version is built using the scss files from the bootstrap source alongside fpp specific scss files

File structure inside the www/css/fpp-bootstrap folder

./src contains the main instruction set in a file called fpp-bootstrap.scss (this is the file that is compiled to create the dist css file which is referenced by the fpp UI)

./src/bootstrap-scss-src this folder holds a copy of the bootstrap scss files taken directly from the bootstrap github repository

## Compiling css via the Live Sass Extension in VS Code 

The required settings for the extension are included in the .vscode/settings.json file
The extension auto creates the dist/fpp-bootstrap.css file when any of the src .scss files are saved
(for this to work click on "Watch Sass" in the blue bottom bar of VSCode - this will change to say "Watching" )

the compiled css file is created and updated in the following folder:

/www/css/fpp-bootstrap/dist-new
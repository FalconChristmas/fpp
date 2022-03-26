#!/usr/bin/env node
//check JSON file

"use strict";
const fs = require("fs");
const json = JSON.parse(fs.readFileSync(process.argv[2]).toString());

//eof

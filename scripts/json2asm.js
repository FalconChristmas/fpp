#!/usr/bin/env node
//xlate JSON file to ASM

"use strict";

const fs = require("fs");

const json = /*JSON.parse*/(fs.readFileSync(process.argv[2] || "/opt/fpp/capes/pi/strings/DPI24Hat.json").toString());

const asm = json.replace(/    /g, " ").split(/ *\r?\n/)
    .map(line => "    DB " + line.split("").map(ch => `'${ch}'`).join(", ") + ", '\\n';")
    .join("\n");
fs.writeFileSync("./json.asm", asm);

//eof

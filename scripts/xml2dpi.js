#!/usr/bin/env node
//xlate FPP Pixel Overlay Models to DPI Pixel Strings

"use strict";

const fs = require("fs");
const xmldoc = require("xmldoc");


//const ovlmods = JSON.parse(fs.readFileSync("/home/fpp/media/config/model-overlays.json").toString());
//console.log(JSON.stringify(ovlmods));

const models = (traverse({}, new xmldoc.XmlDocument(fs.readFileSync(process.argv[2] || "/home/fpp/media/upload/xlights_rgbeffects.xml").toString())) || []).byname.xrgb.byname.models
//console.log("models", JSON.stringify(models, null, 2));
.children.map(model =>
({
    name: model.attr.name,
    starch: model.attr.StartChannel.replace(/![^:]+:/, ""),
    numch: numch(model),
    port: model.byname.ControllerConnection.attr.Port,
    order: model.byname.ControllerConnection.attr.colorOrder || "RGB",
    maxbr: model.byname.ControllerConnection.attr.brightness || 100,
}));

if (false) //debug
models.children.forEach((model, inx) =>
{
//console.log(inx, JSON.stringify(model));
//    console.log(model.byname.StartChannel, model.byname.ChannelCount, model.byname.Name); //, model.byname.ControllerConnection.byname.Port, model.byname.ControllerConnection.byname.Protocol);
console.log(inx, model.attr.name, model.attr.StartChannel.replace(/![^:]+:/, ""), numch(model), model.byname.ControllerConnection.attr.Port);
});

function numch(model)
{
    return model.attr.CustomModel? model.attr.CustomModel.split(/[;,]/).filter(node => node !== "").length:
        model.attr.parm2? +model.attr.parm2:
        0;
}

/*
const models = ovlmods.channelOutputs
    .filter(ovl => ovl.type == "DPIPixels" && ovl.enabled)
    .forEach(ovl => ovl.outputs
        .filter(output => output.protocol == "ws2811")
        .forEach(output =>
({
							"description": "",
							"startChannel": 0,
							"pixelCount": 0,
  console.log(output.portNumber, output.protocol, virtualStrings.length);
  })));
*/

function traverse(parent, child) //recursive
{
//console.log("traverse", JSON.stringify(child, null, 2));
    const newnode =
    {
        name: child.name,
        attr: child.attr, //dictionary
        value: child.val, //string
//        children: [], //array
//        byname: {},
    };
    (parent.children || (parent.children = [])).push(newnode);
    (parent.byname || (parent.byname = {}))[child.name.replace(/[0-9]+$/, "")] = newnode; //remember last node for each unindexed name
    child.eachChild(grandchild => traverse(newnode, grandchild));
    return parent;
}

const pxstrs = JSON.parse(fs.readFileSync("/home/fpp/media/config/co-pixelStrings.json").toString());
//console.log("pxstrs", JSON.stringify(pxstrs, null, 2));
//pxstrs.channelOutputs
//    .filter(chout => chout.type == "DPIPixels")
//    .forEach(chout => chout.outputs.forEach(out =>
//{
//}));
fs.rename("/home/fpp/media/config/co-pixelStrings.json", "/home/fpp/media/config/co-pixelStrings-bk.json", err => console.error("ERROR:", err));

models.forEach(model =>
{
//    name, starch, numch, port
    for (const chout of pxstrs.channelOutputs)
    {
        if (chout.type != "DPIPixels") continue;
        for (const port of chout.outputs)
        {
            if (port.portNumber != model.port - 1) continue;
            for (const virtstr of port.virtualStrings)
            {
                if (virtstr.pixelCount) continue;
                virtstr.description = model.name;
                virtstr.startChannel = +model.starch - 1;
                virtstr.pixelCount = +model.numch;
                virtstr.colorOrder = model.order;
                virtstr.brightness = +model.maxbr;
                return;
            }
            const virtstr = JSON.parse(JSON.stringify(port.virtualStrings[0]));
            virtstr.description = model.name;
            virtstr.startChannel = +model.starch - 1;
            virtstr.pixelCount = +model.numch;
            virtstr.colorOrder = model.order;
            virtstr.brightness = +model.maxbr;
            port.virtualStrings.push(virtstr);
            return;
        }
        console.log("unkn port:", model.port, model.name);
    }
});
for (const chout of pxstrs.channelOutputs)
{
    if (chout.type != "DPIPixels") continue;
    for (const port of chout.outputs)
        port.virtualStrings.sort((lhs, rhs) => lhs.startChannel - rhs.startChannel);
}
fs.writeFileSync("/home/fpp/media/config/co-pixelStrings.json", JSON.stringify(pxstrs, null, 4).replaceAll("    ", "\t"));

//eof

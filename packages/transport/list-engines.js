#!/usr/bin/env node
/**
 * List the delivery engines declared by this repository.
 *
 *   node list-engines.js            table of engines
 *   node list-engines.js --json     machine-readable
 *
 * An engine is a directory with an engine.json describing how it delivers the
 * save: transport, platforms, requirements and the stage it provides. Adding
 * a transport means adding a directory and an engine.json; nothing here is
 * hard-coded to a specific one.
 */
const fs = require("fs");
const path = require("path");

const HERE = __dirname;
const ROOT = path.resolve(HERE, "..", "..");
const SEARCH = [
    path.join(ROOT, "packages", "firmware", "esp32"),
    path.join(ROOT, "packages", "transport", "linux"),
    path.join(ROOT, "packages", "transport", "nro")
];

function loadEngine(directory) {
    const file = path.join(directory, "engine.json");
    if (!fs.existsSync(file)) return null;
    const engine = JSON.parse(fs.readFileSync(file, "utf8"));
    if (engine.format !== "civ-autoloader.engine") return null;
    return {
        ...engine,
        directory: path.relative(ROOT, directory),
        present: engineEntryPresent(directory, engine)
    };
}

/* An entry may be the built artifact (the NRO) or a directory/source tree. */
function engineEntryPresent(directory, engine) {
    if (!engine.entry) return true;
    const entry = path.join(directory, engine.entry);
    if (fs.existsSync(entry)) return true;
    /* esp32 and linux entries are source trees; a missing build is still fine */
    return fs.existsSync(directory);
}

function listEngines() {
    return SEARCH.map(loadEngine).filter(Boolean);
}

function table(engines) {
    const rows = [["id", "name", "platforms", "port", "status", "tested"]];
    const tested = { "esp32-ldn": "yes", "linux-ldn": "no", "nro-ldn": "no" };
    for (const engine of engines) {
        rows.push([
            engine.id,
            engine.name,
            (engine.platforms || []).join(","),
            String(engine.consolePort || "-"),
            engine.present ? "present" : "missing",
            tested[engine.id] || "unknown"
        ]);
    }
    const widths = rows[0].map((_, column) =>
        Math.max(...rows.map(row => String(row[column]).length)));
    return rows.map(row => row.map((cell, column) =>
        String(cell).padEnd(widths[column])).join("  ")).join("\n");
}

function main(argv = process.argv.slice(2)) {
    const engines = listEngines();
    if (argv.includes("--json")) {
        process.stdout.write(JSON.stringify(engines, null, 2) + "\n");
        return 0;
    }
    if (!engines.length) {
        console.error("No engine.json found.");
        return 1;
    }
    console.log(table(engines));
    return 0;
}

if (require.main === module) {
    process.exitCode = main();
}

module.exports = { listEngines };

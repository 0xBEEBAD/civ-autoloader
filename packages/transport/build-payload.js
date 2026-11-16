#!/usr/bin/env node
/**
 * Build the chat payload every transport delivers.
 *
 *   core.js (CivSwitch userland loader)
 *     + packages/firmware/esp32/tools/udp_loader.js  (UDP :9027 receiver)
 *     -> terser -> deflateRaw -> base25 -> chat-safe <img/onerror> lines
 *
 * Usage:
 *   node build-payload.js [--core path] [--loader path] [--out lines.json]
 *
 * Prints the line count. Without --out, writes the JSON array to stdout.
 */
const fs = require("fs");
const os = require("os");
const path = require("path");

const HERE = __dirname;
const PROJECT = path.resolve(HERE, "..", "..");
const DEFAULT_CORE = path.join(PROJECT, "third_party", "CivSwitch", "core.js");
const DEFAULT_LOADER = path.join(PROJECT, "packages", "firmware", "esp32",
    "tools", "udp_loader.js");
const COMPRESS = path.join(HERE, "compress.js");
const ENTRY = "        remote_js_loader();";
const MAX = 1000;

// compress.js takes the source as an argument; it only needs terser.
function loadCompress() {
    const modules = path.join(PROJECT, "third_party", "CivSwitch",
        "PicoSwitch", "node_modules");
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), "civ-compress-"));
    fs.copyFileSync(COMPRESS, path.join(temp, "compress.js"));
    if (fs.existsSync(modules))
        fs.symlinkSync(modules, path.join(temp, "node_modules"));
    return { compress: require(path.join(temp, "compress.js")), temp };
}

function parseArgs(argv) {
    const opts = { core: DEFAULT_CORE, loader: DEFAULT_LOADER };
    const values = { "--core": "core", "--loader": "loader", "--out": "out" };
    for (let i = 0; i < argv.length; i++) {
        const arg = argv[i];
        if (values[arg]) {
            if (!argv[i + 1]) throw new Error(`Missing value for ${arg}`);
            opts[values[arg]] = argv[++i];
        } else if (arg === "--plain") {
            // Leave remote_js_loader() in place: the loader opens TCP :9026.
            opts.plain = true;
        } else if (arg === "--help" || arg === "-h") {
            opts.help = true;
        } else {
            throw new Error(`Unknown argument: ${arg}`);
        }
    }
    return opts;
}

async function build({ core, loader, plain } = {}) {
    const corePath = core || DEFAULT_CORE;
    const source = fs.readFileSync(corePath, "utf8");
    let combined = source;
    if (!plain) {
        const loaderPath = loader || DEFAULT_LOADER;
        if (source.split(ENTRY).length !== 2)
            throw new Error(`Entry point ${JSON.stringify(ENTRY)} not found in ${corePath}`);
        combined = source.replace(ENTRY, fs.readFileSync(loaderPath, "utf8"));
    }

    const { compress, temp } = loadCompress();
    try {
        const lines = await compress(combined);
        for (const line of lines) {
            if (line.length + 1 > MAX)
                throw new Error(`Chat line is ${line.length + 1} bytes (limit ${MAX})`);
        }
        return lines;
    } finally {
        fs.rmSync(temp, { recursive: true, force: true });
    }
}

async function main(argv = process.argv.slice(2)) {
    const opts = parseArgs(argv);
    if (opts.help) {
        console.log("Usage: node build-payload.js [--core path] [--loader path] [--out lines.json] [--plain]");
        return 0;
    }
    const lines = await build(opts);
    const json = JSON.stringify(lines);
    if (opts.out) {
        fs.writeFileSync(opts.out, json);
        console.error(`chat payload: ${lines.length} lines -> ${opts.out}`);
    } else {
        process.stdout.write(json);
    }
    return 0;
}

if (require.main === module) {
    main().then(code => { process.exitCode = code; }).catch(error => {
        console.error(`Error: ${error.message || error}`);
        process.exitCode = 1;
    });
}

module.exports = { build };

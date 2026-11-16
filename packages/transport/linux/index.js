#!/usr/bin/env node
/**
 * Linux entry point: inject the civ-autoloader save over LDN.
 *
 * The LDN protocol layer under ldn/ is CivSwitchLDN's (GPL-3.0-only). This
 * driver supplies our own payloads: the persistent autoloader save.
 *
 * Usage:
 *   node index.js [--keys prod.keys] [--phy phy0] [--host MAC] \
 *                 [--blobs DIR] [--verbose]
 */
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawn } = require("child_process");
const { build } = require("../build-payload");

const HERE = __dirname;
const PROJECT = path.resolve(HERE, "..", "..", "..");
const BLOBS = path.join(PROJECT, "packages", "firmware", "esp32", "blobs");
const PYTHON = path.join(HERE, "ldn", ".venv", "bin", "python-ldn");

function usage() {
    return `Usage: node index.js [options]

  --keys <file>      prod.keys (default ./prod.keys)
  --phy <radio>      Wi-Fi radio (auto-detected when there is one)
  --host <mac>       LDN host MAC (auto-selected when there is one network)
  --ifname <name>    Temporary LDN interface (default civ7-ldn)
  --blobs <dir>      Directory with blob_tpl.bin and blob_apply.bin
  --verbose          Packet and diagnostic logs
  --help             Show this text

Build the blobs first:
  cd packages/firmware/esp32 && python3 tools/build_and_flash.py --keys prod.keys`;
}

function parseArgs(argv) {
    const opts = {
        keys: path.join(HERE, "prod.keys"),
        ifname: "civ7-ldn",
        blobs: BLOBS
    };
    const values = {
        "--keys": "keys", "--phy": "phy", "--host": "host",
        "--ifname": "ifname", "--blobs": "blobs"
    };
    for (let i = 0; i < argv.length; i++) {
        const arg = argv[i];
        if (values[arg]) {
            if (!argv[i + 1] || argv[i + 1].startsWith("--"))
                throw new Error(`Missing value for ${arg}`);
            opts[values[arg]] = argv[++i];
        } else if (arg === "--verbose") {
            opts.verbose = true;
        } else if (arg === "--help" || arg === "-h") {
            opts.help = true;
        } else {
            throw new Error(`Unknown argument: ${arg}`);
        }
    }
    return opts;
}

function selectPhy(phy) {
    if (phy) return phy;
    const dir = "/sys/class/ieee80211";
    if (!fs.existsSync(dir))
        throw new Error("No Wi-Fi radio found. LDN needs a Linux Wi-Fi driver.");
    const radios = fs.readdirSync(dir);
    if (radios.length !== 1)
        throw new Error(`Wi-Fi radios: ${radios.join(", ") || "none"}. Pick one with --phy.`);
    return radios[0];
}

// Our blobs are `u32 length + base64`. The LDN lobby reads raw base64 text.
function prepareAssets(opts) {
    const tpl = path.join(opts.blobs, "blob_tpl.bin");
    const apply = path.join(opts.blobs, "blob_apply.bin");
    for (const file of [tpl, apply]) {
        if (!fs.existsSync(file))
            throw new Error(`Missing ${file}. Build the blobs first (see --help).`);
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "civ-autoloader-"));
    const raw = file => fs.readFileSync(file).subarray(4).toString("ascii");
    fs.writeFileSync(path.join(dir, "template.b64"), raw(tpl), "ascii");
    fs.writeFileSync(path.join(dir, "apply.b64"), raw(apply), "ascii");
    return dir;
}

function run(command, args, options = {}) {
    return new Promise((resolve, reject) => {
        const child = spawn(command, args, {
            cwd: HERE,
            stdio: [options.input === undefined ? "inherit" : "pipe", "inherit", "inherit"]
        });
        const stop = () => child.kill("SIGTERM");
        process.on("SIGINT", stop);
        process.on("SIGTERM", stop);
        const cleanup = () => {
            process.off("SIGINT", stop);
            process.off("SIGTERM", stop);
        };
        child.once("error", error => { cleanup(); reject(error); });
        child.once("close", (code, signal) => {
            cleanup();
            if (code !== 0)
                reject(new Error(`${path.basename(command)} exited with ${signal || `code ${code}`}`));
            else resolve();
        });
        if (options.input !== undefined) {
            child.stdin.on("error", error => {
                if (error.code !== "EPIPE") reject(error);
            });
            child.stdin.end(options.input);
        }
    });
}

async function main(argv = process.argv.slice(2)) {
    const opts = parseArgs(argv);
    if (opts.help) {
        console.log(usage());
        return 0;
    }
    if (!fs.existsSync(PYTHON))
        throw new Error("Run npm install once in packages/transport/linux (creates ldn/.venv).");
    if (!fs.existsSync(opts.keys))
        throw new Error(`Put your own prod.keys at ${opts.keys}.`);

    const phy = selectPhy(opts.phy);
    const assets = prepareAssets(opts);
    try {
        console.log("Building the chat payload...");
        const lines = await build();
        console.log(`Payload ready (${lines.length} chat messages).`);

        const args = [
            path.join(HERE, "bash", "network.sh"),
            "--phy", phy,
            PYTHON, path.join(HERE, "ldn", "main.py"),
            "--keys", path.resolve(opts.keys),
            "--phy", phy,
            "--ifname", opts.ifname,
            "--installer-template", path.join(assets, "template.b64"),
            "--installer-apply", path.join(assets, "apply.b64")
        ];
        if (opts.host) args.push("--host", opts.host);
        if (opts.verbose) args.push("--verbose");

        console.log("Starting LDN delivery. Keep the Civ VII wireless lobby open.");
        await run("bash", args, { input: lines.join("\n") + "\n" });
        console.log("");
        console.log("Save installed. On the console:");
        console.log("  1. Close Civ VII completely.");
        console.log("  2. Reopen it and choose Load Game.");
        console.log("  3. Load XSTRIG.Civ7Save.");
        return 0;
    } finally {
        fs.rmSync(assets, { recursive: true, force: true });
    }
}

if (require.main === module) {
    main().then(code => { process.exitCode = code; }).catch(error => {
        console.error(`Error: ${error.message || error}`);
        process.exitCode = 1;
    });
}

module.exports = { main };

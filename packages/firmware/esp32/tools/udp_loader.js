        // ==== UDP payload channel (LDN, fully offline) ====
        // Safe bulk read: one read64 per 8 bytes (read8 does a full read64 anyway).
        function fast_str(addr, len) {
            const codes = new Array(len);
            let i = 0;
            for (; i + 8 <= len; i += 8) {
                const v = read64(addr + BigInt(i));
                codes[i]     = Number(v & 0xffn);
                codes[i + 1] = Number((v >> 8n) & 0xffn);
                codes[i + 2] = Number((v >> 16n) & 0xffn);
                codes[i + 3] = Number((v >> 24n) & 0xffn);
                codes[i + 4] = Number((v >> 32n) & 0xffn);
                codes[i + 5] = Number((v >> 40n) & 0xffn);
                codes[i + 6] = Number((v >> 48n) & 0xffn);
                codes[i + 7] = Number((v >> 56n) & 0xffn);
            }
            for (; i < len; i++) codes[i] = Number(read8(addr + BigInt(i)));
            let s = "";
            for (let k = 0; k < len; k += 4096)
                s += String.fromCharCode.apply(null, codes.slice(k, k + 4096));
            return s;
        }

        function b64d(s) {
            const A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            const m = {};
            for (let i = 0; i < 64; i++) m[A[i]] = i;
            const o = [];
            let i = 0;
            while (i + 3 < s.length) {
                const c0 = m[s[i]], c1 = m[s[i+1]];
                const p2 = s[i+2] === "=", p3 = s[i+3] === "=";
                const c2 = p2 ? 0 : m[s[i+2]], c3 = p3 ? 0 : m[s[i+3]];
                const v = (c0 << 18) | (c1 << 12) | (c2 << 6) | c3;
                o.push((v >> 16) & 255);
                if (!p2) o.push((v >> 8) & 255);
                if (!p2 && !p3) o.push(v & 255);
                i += 4;
            }
            let r = "";
            for (let k = 0; k < o.length; k++) r += String.fromCharCode(o[k]);
            return r;
        }

        async function core_udp_loader() {
            const UD = 9027;
            const fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd === -1n) { await log("udp socket failed"); return; }
            const rcvbuf = malloc(4);
            write32(rcvbuf, 512 * 1024);
            setsockopt(fd, SOL_SOCKET, 0x1002, rcvbuf, 4);   /* SO_RCVBUF */
            for (let i = 0; i < 16; i++) write8(sockaddr_in + BigInt(i), 0);
            write8(sockaddr_in, 16n);
            write8(sockaddr_in + 1n, 2n);
            write16(sockaddr_in + 2n, BigInt(((UD & 0xff) << 8) | (UD >> 8)));
            write32(sockaddr_in + 4n, 0n);
            if (bind(fd, sockaddr_in, 16) === -1n) { await log("udp bind failed"); return; }
            await log("UDP PAYLOAD CHANNEL :9027");

            const buf = malloc(2048);
            const tx = malloc(512);
            const tpl = [], js = [], cfs = [], start = [];
            let stages = ["persist"];
            let needT = 0, needJ = 0, needC = 0, needS = 0;
            let gotT = 0, gotJ = 0, gotC = 0, gotS = 0;
            let rx = 0, connected = false, finished = false;

            function udp_tx(text) {
                const len = text.length;
                for (let i = 0; i < len; i++) write8(tx + BigInt(i), BigInt(text.charCodeAt(i)));
                send(fd, tx, len, 0);
            }

            // Read the console timezone through the game SDK once, before
            // the CFS payload runs. Silent on failure: UTC is an acceptable
            // fallback and must never block the install pipeline.
            async function probe_game_time() {
                try {
                    call_rop(sdk_base + 0x14c960n);              // SM Initialize
                    const out = malloc(8), name = malloc(16);
                    write32(out, 0n);
                    write32(name, 0x656d6974n);                  // "time"
                    write32(name + 4n, 0x0000753an);             // ":u\0"
                    call_rop(sdk_base + 0x14cab0n, out, name);   // SM GetService
                    const timeHandle = Number(read32(out)) >>> 0;
                    if (!timeHandle) return;
                    function reply(p) {
                        const t0 = Number(read32(p)) >>> 0;
                        const t1 = Number(read32(p + 4n)) >>> 0;
                        let at = 8; const copies = [], moves = [];
                        if (t1 >>> 31) {
                            const sp = Number(read32(p + 8n)) >>> 0; at = 12;
                            if (sp & 1) at += 8;
                            let n = (sp >>> 1) & 15;
                            for (let i = 0; i < n; i++) { copies.push(Number(read32(p + BigInt(at))) >>> 0); at += 4; }
                            n = (sp >>> 5) & 15;
                            for (let i = 0; i < n; i++) { moves.push(Number(read32(p + BigInt(at))) >>> 0); at += 4; }
                        }
                        at += (t0 >>> 16 & 15) * 8;
                        at += ((t0 >>> 20 & 15) + (t0 >>> 24 & 15) + (t0 >>> 28 & 15)) * 12;
                        at = (at + 15) & ~15;
                        return {p, result: Number(read32(p + BigInt(at + 8))) >>> 0,
                            data: at + 16, copies, moves};
                    }
                    function send(handle, command, posix) {
                        const p = malloc(0x1000);
                        for (let i = 0; i < 0x40; i += 4) write32(p + BigInt(i), 0n);
                        write32(p, 4n);
                        write32(p + 4n, posix === undefined ? 8n : 10n);
                        write32(p + 0x10n, 0x49434653n);
                        write32(p + 0x18n, BigInt(command));
                        if (posix !== undefined) {
                            write32(p + 0x20n, BigInt(posix % 4294967296));
                            write32(p + 0x24n, BigInt(Math.floor(posix / 4294967296)));
                        }
                        call_rop(sdk_base + 0x14ba90n, BigInt(handle), p, 0x1000n);
                        return reply(p);
                    }
                    const clockReply = send(timeHandle, 0);
                    const tzReply = send(timeHandle, 3);
                    const clock = clockReply.moves[0] || clockReply.copies[0];
                    const tz = tzReply.moves[0] || tzReply.copies[0];
                    if (!clock || !tz) return;
                    const now = send(clock, 0);
                    const posix = Number(read32(now.p + BigInt(now.data))) +
                        Number(read32(now.p + BigInt(now.data + 4))) * 4294967296;
                    const cal = send(tz, 101, posix);
                    const offset = Number(read32(cal.p + BigInt(cal.data + 0x1C))) | 0;
                    if (cal.result || !offset) return;
                    globalThis.__AUTOLOADER_TZ_OFFSET = -offset / 60;
                } catch (e) {}
            }
            // Mirror the loader console to the ESP32 serial log. The board
            // receives "L<line>" on :9027 and prints it on its USB-Serial/JTAG.
            const _relay_old_log = window.log;
            let _relay_seq = 0;
            window.log = function (message) {
                try {
                    let line = String(message).replace(/\u0001stage:/g, "")
                        .replace(/[\r\n]+/g, " ");
                    if (line.length > 380) line = line.slice(0, 380);
                    if (line) {
                        const packet = "L" + (++_relay_seq) + ":" + line;
                        // UDP log lines are best-effort; repeat the same
                        // sequence number so the board can drop duplicates.
                        for (let i = 0; i < 3; i++) udp_tx(packet);
                    }
                } catch (e) {}
                return _relay_old_log.apply(this, arguments);
            };

            await probe_game_time();

            async function eval_payload(label, base64) {
                const src = b64d(base64);
                await log("RUNNING " + label + " (" + src.length + " bytes)");
                try { await eval(src); } catch (e) {
                    await log("E: " + e.message);
                    await log(e.stack);
                }
                await log(label + " RETURNED");
            }

            async function finish_install() {
                if (finished) return;
                finished = true;
                for (let k = 0; k < 3; k++) udp_tx("D");
                if (stages.includes("dev"))
                    globalThis.__CIVSWITCH_DEV__ = true;
                for (const stage of stages) {
                    if (stage === "dev") {
                        continue;
                    } else if (stage === "persist") {
                        if (needJ && gotJ >= needJ && needT && gotT >= needT) {
                            await eval_payload("PERSIST", js.slice(0, needJ).join(""));
                            if (!window.__cfsPersistInstaller) {
                                await log("E: persist installer unavailable");
                                continue;
                            }
                            const token = "ldn";
                            for (let i = 0; i < needT; i++) {
                                const part = tpl[i] || "";
                                if (!part) continue;
                                window.__cfsPersistInstaller.stageChunk(
                                    token, i, i * 1100, part);
                            }
                            await window.__cfsPersistInstaller.installFromTemplate();
                            await log("PERSIST INSTALL DONE - restart the game, then Load Game");
                        } else {
                            await log("stage persist skipped (missing blobs)");
                        }
                    } else if (stage === "save") {
                        if (needJ) {
                            window.__tpl = tpl.slice(0, needT);
                            await eval_payload("INSTALLER", js.slice(0, needJ).join(""));
                        } else {
                            await log("stage save skipped (no blob)");
                        }
                    } else if (stage === "cfs") {
                        if (needC && gotC >= needC) {
                            await log("SD card must contain hbmenu.nro, bootstrap.bin and kaihen/bin/...");
                            await eval_payload("CIVSWITCHFS", cfs.slice(0, needC).join(""));
                        } else {
                            await log("stage cfs skipped (no blob)");
                        }
                    } else if (stage === "gate") {
                        const r = globalThis.__AUTOLOADER_RESULT;
                        await log("stage gate " + JSON.stringify(r || null));
                    } else if (stage === "hen") {
                        if (needS && gotS >= needS) {
                            await eval_payload("START", start.slice(0, needS).join(""));
                        } else {
                            await log("stage hen skipped (no blob)");
                        }
                    } else if (stage === "tcp") {
                        await log("leaving ldn network.");
                        await log("switching to TCP loader :9026");
                        await remote_js_loader();
                    } else {
                        await log("unknown stage " + stage);
                    }
                }
                await log("pipeline complete");
            }

            async function try_finish() {
                if (finished) return;
                if (needT && gotT < needT) return;
                if (needJ && gotJ < needJ) return;
                if (needC && gotC < needC) return;
                if (needS && gotS < needS) return;
                if (!needT && !needJ && !needC && !needS) return;
                await finish_install();
            }

            function request_missing() {
                let n = 0;
                for (let i = 0; i < needT && n < 96; i++)
                    if (!tpl[i]) { udp_tx("t" + i); n++; }
                for (let i = 0; i < needJ && n < 96; i++)
                    if (!js[i]) { udp_tx("j" + i); n++; }
                for (let i = 0; i < needC && n < 96; i++)
                    if (!cfs[i]) { udp_tx("k" + i); n++; }
                for (let i = 0; i < needS && n < 96; i++)
                    if (!start[i]) { udp_tx("s" + i); n++; }
                return n;
            }

            for (;;) {
                const n = Number(recv(fd, buf, 2048, 0));
                if (n <= 0) continue;
                rx++;
                if (finished) continue;
                let s;
                try { s = fast_str(buf, n); } catch (e) { s = ""; }
                if (!s) continue;
                try {
                    const c = s[0], p = s.indexOf(':');
                    if (c === 'M') {
                        stages = s.slice(1).split(',').filter(Boolean);
                        await log("stages " + stages.join(","));
                        continue;
                    }
                    if (c === 'T' || c === 'J' || c === 'K' || c === 'S') {
                        const arr = c === 'T' ? tpl : c === 'J' ? js : c === 'K' ? cfs : start;
                        const i = +s.slice(1, p);
                        if (!arr[i]) {
                            arr[i] = s.slice(p + 1);
                            if (c === 'T') gotT++;
                            else if (c === 'J') gotJ++;
                            else if (c === 'K') gotC++;
                            else gotS++;
                        }
                        await try_finish();
                    } else if (c === 'A') {
                        const p2 = s.indexOf(':', p + 1);
                        const p3 = p2 > 0 ? s.indexOf(':', p2 + 1) : -1;
                        const p4 = p3 > 0 ? s.indexOf(':', p3 + 1) : -1;
                        needT = +s.slice(1, p);
                        needJ = p2 > 0 ? +s.slice(p + 1, p2) : 0;
                        needC = p3 > 0 ? +s.slice(p2 + 1, p3) : 0;
                        needS = p4 > 0 ? +s.slice(p3 + 1, p4) : 0;
                        const ipText = p4 > 0 ? s.slice(p4 + 1) : (p3 > 0 ? s.slice(p3 + 1) : (p2 > 0 ? s.slice(p2 + 1) : ""));
                        if (ipText) globalThis.__AUTOLOADER_PEER_IP = ipText;
                        if (ipText && !connected) {
                            const ip = ipText.split('.');
                            if (ip.length === 4) {
                                for (let i = 0; i < 16; i++) write8(sockaddr_in + BigInt(i), 0);
                                write8(sockaddr_in, 16n);
                                write8(sockaddr_in + 1n, 2n);
                                write16(sockaddr_in + 2n, BigInt(((UD & 0xff) << 8) | (UD >> 8)));
                                write32(sockaddr_in + 4n,
                                    BigInt(+ip[0]) | (BigInt(+ip[1]) << 8n) |
                                    (BigInt(+ip[2]) << 16n) | (BigInt(+ip[3]) << 24n));
                                connected = connect(fd, sockaddr_in, 16) === 0n;
                            }
                        }
                        await log("UDP header tpl=" + needT + " js=" + needJ +
                                  " cfs=" + needC + " s=" + needS +
                                  (connected ? " (nak)" : ""));
                    } else if (c === 'G') {
                        if (!connected) {
                            await log("UDP no return path; cannot request missing chunks");
                            continue;
                        }
                        const miss = request_missing();
                        if (miss) {
                            await log("UDP repairing " + miss + " chunk(s) rx=" + rx);
                        } else {
                            await try_finish();
                        }
                    }
                } catch (e) { await log("udp rx: " + e.message); }
            }
        }
        core_udp_loader();

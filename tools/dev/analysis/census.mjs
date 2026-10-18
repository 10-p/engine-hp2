// census.mjs <file.wasm>... -- per-function local counts against the browser's per-function limit (the frontend's
// wasm-limits.mjs): function count, how many are over / near the limit, and the 12 with the most locals.
import { readFileSync } from 'node:fs';
import { checkWasmLimits, localCensus } from '/home/ion/WorkUnreal/unreal-frontend-arch-private/packages/build/src/wasm-limits.mjs';
for (const f of process.argv.slice(2)) {
  const buf = readFileSync(f);
  const rows = localCensus(buf);
  const r = checkWasmLimits(buf);
  console.log('==', f, 'functions', rows.length, 'over', r.over.length, 'near', r.near.length);
  for (const x of rows.slice(0, 12)) console.log('  ', x.locals, x.name.slice(0, 110));
}

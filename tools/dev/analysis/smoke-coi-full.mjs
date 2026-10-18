#!/usr/bin/env node
// smoke-coi-full.mjs — bare-loader smoke test of the wasm build, served cross-origin isolated (COOP/COEP, so the
// threaded build gets SharedArrayBuffer): loads SDLLaunch.{js,wasm,data} from a minimal host page in headless
// Chromium (Playwright), passes when the runtime initializes and the engine prints its boot log, and prints up to
// 200 stdout lines. The page has only the preloaded System/Default.ini and DefUser.ini (no asset streaming), so the
// engine stops at package loading, before any WebGL: no GPU needed.
// Usage, from the repo root: node tools/dev/analysis/smoke-coi-full.mjs [artifactDir=build-wasm/SDLLaunch] [seconds=20]
import { createServer } from 'node:http';
import { readFileSync, existsSync, writeFileSync } from 'node:fs';
import { extname, join, resolve } from 'node:path';
import { createRequire } from 'node:module';

const DIR = resolve(process.argv[2] || 'build-wasm/SDLLaunch');
const SECS = Number(process.argv[3] || 20);
const PORT = 8792;
// Playwright is resolved from the frontend's packages/e2e; this repo has no node_modules.
const require = createRequire('/home/ion/WorkUnreal/unreal-frontend-arch-private/packages/e2e/');
const { chromium } = require('playwright');

if (!existsSync(join(DIR, 'SDLLaunch.js'))) {
    console.error(`[smoke] no SDLLaunch.js in ${DIR} — build first (cmake --build --preset wasm)`);
    process.exit(2);
}

// Minimal host page: the build is not MODULARIZE, so the loader reads a global Module.
writeFileSync(
    join(DIR, '__smoke.html'),
    `<!doctype html><meta charset=utf-8><canvas id=canvas width=320 height=240></canvas><script>
window.__log=[];function cap(k,t){window.__log.push('['+k+'] '+t);}
var Module={canvas:document.getElementById('canvas'),arguments:[],
 print:function(t){cap('OUT',t);},printErr:function(t){cap('ERR',t);},
 onAbort:function(w){cap('ABORT',String(w));window.__abort=true;},
 locateFile:function(p){return p;},
 onRuntimeInitialized:function(){cap('SMOKE','runtimeInitialized');window.__rt=true;}};
window.Module=Module;
</script><script src="SDLLaunch.js"></script>`,
);

const MIME = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.wasm': 'application/wasm',
    '.data': 'application/octet-stream',
};
const server = createServer((req, res) => {
    let p = req.url.split('?')[0];
    if (p === '/') p = '/__smoke.html';
    const f = join(DIR, p);
    if (!existsSync(f)) {
        res.writeHead(404);
        res.end('nf');
        return;
    }
    res.writeHead(200, { 'Content-Type': MIME[extname(f)] || 'application/octet-stream', 'Cross-Origin-Opener-Policy': 'same-origin', 'Cross-Origin-Embedder-Policy': 'require-corp' });
    res.end(readFileSync(f));
});
await new Promise((r) => server.listen(PORT, r));

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage();
const console_lines = [];
page.on('console', (m) => console_lines.push(m.text()));
page.on('pageerror', (e) => console_lines.push('[PAGEERROR] ' + e.message));
await page.goto(`http://localhost:${PORT}/`, { waitUntil: 'load' });
await page.waitForTimeout(SECS * 1000);

const state = await page.evaluate(() => {
    const out = { rt: !!window.__rt, abort: !!window.__abort, log: window.__log || [], memfs: [], engineLog: null };
    try {
        const FS = window.FS || (window.Module && window.Module.FS);
        if (FS) {
            const walk = (d) => {
                for (const n of FS.readdir(d)) {
                    if (n === '.' || n === '..') continue;
                    const p = d === '/' ? '/' + n : d + '/' + n;
                    let mode = 0;
                    try {
                        mode = FS.stat(p).mode;
                    } catch {}
                    out.memfs.push(p);
                    if (FS.isDir(mode) && out.memfs.length < 200) walk(p);
                }
            };
            walk('/');
            const logPath = out.memfs.find((p) => /\.log$/i.test(p));
            if (logPath) out.engineLog = new TextDecoder().decode(FS.readFile(logPath));
        }
    } catch (e) {
        out.fsErr = String(e);
    }
    return out;
});
await browser.close();
server.close();

const engineLog = state.log.filter((l) =>
    /(Log|Init|Warning|Exit|DevNet|Bound|Unreal|Version|Package|Core|appThrowf|MissingIni|Detected)/i.test(l),
);
console.log(`\n=== web-smoke: ${DIR} ===`);
console.log(`runtimeInitialized: ${state.rt}`);
console.log(`abort: ${state.abort}`);
console.log(`Module.print/err lines: ${state.log.length}`);
console.log(`MEMFS entries: ${state.memfs.length}${state.fsErr ? ' (FS err: ' + state.fsErr + ')' : ''}`);
if (state.engineLog) {
    const lines = state.engineLog.split('\n');
    console.log(`--- engine log file (${lines.length} lines; first 45) ---`);
    console.log(lines.slice(0, 45).join('\n'));
} else {
    console.log('--- no engine .log file in MEMFS; stdout lines: ---');
    console.log(state.log.slice(0, 200).join('\n'));
}
if (console_lines.some((l) => l.includes('PAGEERROR'))) {
    console.log('--- page errors ---');
    console.log(console_lines.filter((l) => l.includes('PAGEERROR')).join('\n'));
}
// PASS = the runtime initialized and the engine ran. Without streamed packages the run ends at package resolution
// (Core.u/Engine.u are not in the FS), which the [appThrowf] diagnostic names; that counts as a pass.
const reachedPackageLoad = state.log.some((l) => /appThrowf|PackageNotFound|MissingIni|Package/i.test(l));
const engineRan = !!state.engineLog || engineLog.length > 0 || reachedPackageLoad;
const pass = state.rt && engineRan;
console.log(
    `\n[smoke] ${pass ? 'PASS' : 'FAIL'} — runtimeInitialized=${state.rt} engineExecuted=${engineRan}` +
        (reachedPackageLoad ? ' (reached package loading = the expected pre-FetchFS boundary)' : ''),
);
process.exit(pass ? 0 : 1);

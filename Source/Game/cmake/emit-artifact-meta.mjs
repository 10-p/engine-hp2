#!/usr/bin/env node
// emit-artifact-meta.mjs — write <out>/meta.json, the descriptor of the just-linked WASM artifact.
//   node emit-artifact-meta.mjs --out=<dir> --target=<id> --abi=<n> [--gitSha=<sha>] [--emcc=<ver>] [--dep-<label>=<repo>@<ref> ...]
// Run by the Game target's POST_BUILD after SDLLaunch.js is final, so the js checksum is of the served file.
// Node stdlib only, so the artifact and its descriptor travel together; the consumer validates it against the
// glue ABI_VERSION and the host loader contract.
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const args = Object.fromEntries(
    process.argv.slice(2).map((a) => {
        const i = a.indexOf('=');
        return [a.slice(2, i), a.slice(i + 1)];
    }),
);
const dir = args.out;

// Provenance is resolved here, at build time: the values on the POST_BUILD command line are computed once per
// configure, so a plain rebuild would stamp a stale SHA. The passed --gitSha is the fallback for a tree without
// git; --builtAt is not read.
const _here = dirname(fileURLToPath(import.meta.url));
const gitSha = (() => {
    try {
        return execFileSync('git', ['-C', _here, 'rev-parse', '--short', 'HEAD'], {
            encoding: 'utf8',
            stdio: ['ignore', 'pipe', 'ignore'],
        }).trim();
    } catch {
        return args.gitSha || 'unknown';
    }
})();
const builtAt = new Date().toISOString().replace(/\.\d+Z$/, 'Z');

const sha = (f) =>
    existsSync(join(dir, f)) ? 'sha256-' + createHash('sha256').update(readFileSync(join(dir, f))).digest('hex') : null;

const checksums = { wasm: sha('SDLLaunch.wasm'), js: sha('SDLLaunch.js') };
const dataSha = sha('SDLLaunch.data'); // optional (only if --preload-file produced it)
if (dataSha) checksums.data = dataSha;

// Every fetched library as `<repo>@<branch|tag>` — never a commit, because the branch tip is
// what a rebuild takes — handed over as --dep-<label>=… from the same cache variables FetchContent read
// (cmake/ArtifactDeps.cmake). A GitHub URL is shortened to owner/name; a local checkout override arrives as
// `local:<path>` and is kept verbatim, so such an artifact never claims a provenance it does not have.
const depSpec = (v) => v.replace(/^https:\/\/github\.com\/([^/]+)\/([^/@]+?)(?:\.git)?(@.*)$/, '$1/$2$3');
const deps = Object.fromEntries(
    Object.entries(args)
        .filter(([k]) => k.startsWith('dep-'))
        .map(([k, v]) => [k.slice(4), depSpec(v)]),
);

const meta = {
    targetId: args.target || 'hp2',
    engineFamily: 'UE1',
    buildConfig: 'wasm-integrated',
    abiVersion: Number(args.abi), // from the vendored frozen ue_abi.h UE_ABI_VERSION
    loaderStyle: 'global-module', // classic <script> + bare globals
    provenance: { repo: 'HarryPotter2', gitSha, builtAt }, // must equal the target descriptor's publish.engine.repo
    toolchain: { emscripten: args.emcc || 'unknown', base: 'Harry Potter and the Chamber of Secrets (UE1 build 433), templated on engine-ut99', ...deps },
    buildFlags: [
    // Descriptive, hand-maintained: keep it in step with the link flags in Source/Game/CMakeLists.txt. The
    // threading set makes this a threaded-only artifact, which the host gates on SharedArrayBuffer +
    // OffscreenCanvas.
    '-sASYNCIFY', '-sFULL_ES2', '-sUSE_SDL=2', '-sALLOW_MEMORY_GROWTH', '-sFORCE_FILESYSTEM', '-sWASMFS',
    '-pthread', '-sPROXY_TO_PTHREAD', '-sOFFSCREENCANVAS_SUPPORT', '-sOFFSCREENCANVASES_TO_PTHREAD=#canvas',
    '-sOFFSCREEN_FRAMEBUFFER',
  ],
    exports: { symbolContract: 1 },
    checksums,
};

writeFileSync(join(dir, 'meta.json'), JSON.stringify(meta, null, 2) + '\n');
console.log(`[meta] wrote ${join(dir, 'meta.json')} (abi ${meta.abiVersion}, ${meta.buildConfig}, ${meta.loaderStyle})`);

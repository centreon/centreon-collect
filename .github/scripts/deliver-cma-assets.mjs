#!/usr/bin/env node
/**
 * Delivers CMA assets to a GitHub release and registers them on download.centreon.com.
 *
 * Modes
 *   local  (default): read local files matching --file-pattern
 *   action:           fetch from Artifactory stable (fallback: re-process existing GitHub release assets)
 *
 * Usage (local):
 *   node deliver-cma-assets.mjs \
 *     --github-release centreon-monitoring-agent-27.08.3 \
 *     --file-pattern "/tmp/packages/centreon-monitoring-agent-27.08*.rpm"
 *
 * Usage (action — re-run a failed delivery):
 *   node deliver-cma-assets.mjs \
 *     --mode action \
 *     --github-release centreon-monitoring-agent-27.08.3 \
 *     --version 27.08.3 \
 *     [--distrib el8,el9,el10,windows]
 *
 * Tokens (--flag or matching env var):
 *   GITHUB_TOKEN                  GitHub token with write:packages + contents scope
 *   ARTIFACTORY_ACCESS_TOKEN      JFrog token (action mode)
 *   TOKEN_DOWNLOAD_CENTREON_COM   download.centreon.com registration token
 *
 * Flags:
 *   --dry-run            Log what would happen without executing uploads/registrations
 *   --force-reregister   Re-register on download.centreon.com even if asset is already on the release
 */

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import crypto from 'node:crypto';

// ─── ANSI colours ─────────────────────────────────────────────────────────────

const COLOUR = process.stdout.isTTY;
const c = {
  reset:  COLOUR ? '\x1b[0m'  : '',
  bold:   COLOUR ? '\x1b[1m'  : '',
  dim:    COLOUR ? '\x1b[2m'  : '',
  green:  COLOUR ? '\x1b[32m' : '',
  yellow: COLOUR ? '\x1b[33m' : '',
  blue:   COLOUR ? '\x1b[34m' : '',
  cyan:   COLOUR ? '\x1b[36m' : '',
  red:    COLOUR ? '\x1b[31m' : '',
};

const log = {
  header: (msg) => console.log(`\n${c.bold}${c.blue}◆ ${msg}${c.reset}`),
  info:   (msg) => console.log(`  ${c.dim}ℹ${c.reset}  ${msg}`),
  ok:     (msg) => console.log(`  ${c.green}✓${c.reset}  ${msg}`),
  skip:   (msg) => console.log(`  ${c.dim}→${c.reset}  ${msg}`),
  upload: (msg) => console.log(`  ${c.cyan}↑${c.reset}  ${msg}`),
  warn:   (msg) => console.log(`  ${c.yellow}⚠${c.reset}  ${msg}`),
  error:  (msg) => console.error(`  ${c.red}✗${c.reset}  ${msg}`),
  file:   (name) => console.log(`\n  ${c.bold}${name}${c.reset}`),
};

// ─── CLI + env config ─────────────────────────────────────────────────────────

function parseArgs(argv) {
  const r = {};
  for (let i = 0; i < argv.length; i++) {
    if (!argv[i].startsWith('--')) continue;
    const key  = argv[i].slice(2);
    const next = argv[i + 1];
    if (next !== undefined && !next.startsWith('--')) { r[key] = next; i++; }
    else r[key] = true;
  }
  return r;
}

const args = parseArgs(process.argv.slice(2));

const MODE              = args['mode']              ?? process.env.DELIVER_MODE              ?? 'local';
const FILE_PATTERN      = args['file-pattern']      ?? process.env.DELIVER_FILE_PATTERN;
const GITHUB_RELEASE    = args['github-release']    ?? process.env.DELIVER_GITHUB_RELEASE;
const VERSION           = args['version']           ?? process.env.DELIVER_VERSION;
const DISTRIBS          = (args['distrib']          ?? process.env.DELIVER_DISTRIB ?? '').split(',').filter(Boolean);
const DRY_RUN           = args['dry-run']           === true || process.env.DELIVER_DRY_RUN === 'true';
const FORCE_REREGISTER  = args['force-reregister']  === true;
const REPO              = args['repo']              ?? process.env.DELIVER_REPO ?? process.env.GITHUB_REPOSITORY ?? 'centreon/centreon-collect';
const GITHUB_TOKEN      = args['github-token']      ?? process.env.GITHUB_TOKEN;
const ARTIFACTORY_TOKEN = args['artifactory-token'] ?? process.env.ARTIFACTORY_ACCESS_TOKEN;
const DOWNLOAD_TOKEN    = args['download-token']    ?? process.env.TOKEN_DOWNLOAD_CENTREON_COM;

const [REPO_OWNER, REPO_NAME] = REPO.split('/');

// ─── GitHub REST API ──────────────────────────────────────────────────────────

async function ghFetch(method, endpoint, body) {
  const url = `https://api.github.com${endpoint}`;
  const r = await fetch(url, {
    method,
    headers: {
      'Authorization':        `Bearer ${GITHUB_TOKEN}`,
      'Accept':               'application/vnd.github+json',
      'X-GitHub-Api-Version': '2022-11-28',
      ...(body ? { 'Content-Type': 'application/json' } : {}),
    },
    body: body ? JSON.stringify(body) : undefined,
  });
  if (!r.ok) throw new Error(`GitHub ${method} ${endpoint} → ${r.status}: ${await r.text()}`);
  return r.json();
}

async function getReleaseByTag(tag) {
  return ghFetch('GET', `/repos/${REPO_OWNER}/${REPO_NAME}/releases/tags/${tag}`);
}

async function listReleaseAssets(releaseId) {
  const all = [];
  let page = 1;
  while (true) {
    const batch = await ghFetch('GET', `/repos/${REPO_OWNER}/${REPO_NAME}/releases/${releaseId}/assets?per_page=100&page=${page}`);
    all.push(...batch);
    if (batch.length < 100) break;
    page++;
  }
  return all;
}

async function uploadReleaseAsset(releaseId, fileName, data) {
  const url = `https://uploads.github.com/repos/${REPO_OWNER}/${REPO_NAME}/releases/${releaseId}/assets?name=${encodeURIComponent(fileName)}`;
  const r = await fetch(url, {
    method: 'POST',
    headers: {
      'Authorization':        `Bearer ${GITHUB_TOKEN}`,
      'Content-Type':         'application/octet-stream',
      'X-GitHub-Api-Version': '2022-11-28',
    },
    body: data,
  });
  if (!r.ok) throw new Error(`Upload failed ${r.status}: ${await r.text()}`);
  return r.json();
}

// ─── Artifactory REST API ─────────────────────────────────────────────────────

async function searchArtifactory(version) {
  // AQL search across all CMA-relevant repos for stable packages of the given version
  const aql = [
    'items.find({',
    '  "$or": [',
    '    {"repo": "rpm-standard"},',
    '    {"repo": "apt-standard"},',
    '    {"repo": "ubuntu-standard"},',
    '    {"repo": "installers"}',
    '  ],',
    `  "name": {"$match": "centreon-monitoring-agent*${version}*"},`,
    '  "path": {"$match": "*/stable*"}',
    '}).include("name", "repo", "path", "size")',
  ].join('\n');

  const r = await fetch('https://packages.centreon.com/artifactory/api/search/aql', {
    method: 'POST',
    headers: {
      'Authorization': `Bearer ${ARTIFACTORY_TOKEN}`,
      'Content-Type':  'text/plain',
    },
    body: aql,
  });
  if (!r.ok) throw new Error(`Artifactory AQL ${r.status}: ${await r.text()}`);
  const data = await r.json();
  return data.results ?? [];
}

async function downloadFromArtifactory(repo, artifactoryPath, fileName, destDir) {
  const url = `https://packages.centreon.com/artifactory/${repo}/${artifactoryPath}/${fileName}`;
  log.info(`Downloading: ${url}`);
  const r = await fetch(url, {
    headers: { 'Authorization': `Bearer ${ARTIFACTORY_TOKEN}` },
  });
  if (!r.ok) throw new Error(`Artifactory download ${r.status}: ${url}`);
  const dest = path.join(destDir, fileName);
  fs.writeFileSync(dest, Buffer.from(await r.arrayBuffer()));
  return dest;
}

// ─── download.centreon.com ────────────────────────────────────────────────────

function parseFileMetadata(fileName) {
  const m = fileName.match(/^([\w-]+)(?:-|_)(\d+\.\d+)\.(\d+).*\.(\w+)$/);
  if (!m) return null;
  const [, product, majorVersion, minorVersion, extension] = m;

  let distribSuffix = '';
  const dm = fileName.match(/(el|deb|ubuntu|exe)\.?(\d+(?:\.\d+)?)?/);
  if (dm) {
    switch (dm[1]) {
      case 'el':      distribSuffix = `-${dm[1]}${dm[2]}`;  break;
      case 'deb':     distribSuffix = `-debian-${dm[2]}`;   break;
      case 'ubuntu':  distribSuffix = `-ubuntu-${dm[2]}`;   break;
      case 'exe':     distribSuffix = '-windows';            break;
    }
  }

  let archSuffix = '';
  if (/(amd64|x86_64)/.test(fileName))        archSuffix = '-amd64';
  else if (/(arm64|aarch64)/.test(fileName))  archSuffix = '-arm64';

  return { product, majorVersion, minorVersion, extension, distribSuffix, archSuffix };
}

async function registerOnDownloadCentreon(fileName, localPath, downloadUrl) {
  const meta = parseFileMetadata(fileName);
  if (!meta) {
    log.warn(`Cannot parse metadata from ${fileName} — skipping download.centreon.com`);
    return;
  }
  const { product, majorVersion, minorVersion, extension, distribSuffix, archSuffix } = meta;
  const md5  = crypto.createHash('md5').update(fs.readFileSync(localPath)).digest('hex');
  const size = fs.statSync(localPath).size;

  const params = new URLSearchParams({
    token:       DOWNLOAD_TOKEN,
    product,
    release:     majorVersion,
    version:     `${majorVersion}.${minorVersion}${distribSuffix}${archSuffix}`,
    extension,
    md5,
    size:        String(size),
    ddos:        '0',
    dryrun:      '0',
    release_url: downloadUrl,
  });

  if (DRY_RUN) {
    log.skip(`[dry-run] Would register on download.centreon.com: ${product} ${majorVersion}.${minorVersion}${distribSuffix}${archSuffix}`);
    return;
  }

  const r = await fetch(`https://download.centreon.com/api/?${params}`);
  if (!r.ok) throw new Error(`download.centreon.com ${r.status}: ${await r.text()}`);
  const body = await r.text();
  log.ok(`Registered on download.centreon.com${body ? `: ${body.trim()}` : ''}`);
}

// ─── Simple glob ──────────────────────────────────────────────────────────────

function* globFiles(pattern) {
  // Supports a single * wildcard within a path segment
  const starIdx = pattern.indexOf('*');
  if (starIdx === -1) {
    if (fs.existsSync(pattern)) yield pattern;
    return;
  }
  const before = pattern.slice(0, starIdx);
  const after  = pattern.slice(starIdx + 1);
  const dir    = path.dirname(before) || '.';
  const prefix = path.basename(before);
  const suffix = after.includes('/') ? after.slice(0, after.indexOf('/')) : after;

  try {
    for (const name of fs.readdirSync(dir)) {
      if (name.startsWith(prefix) && name.endsWith(suffix)) {
        yield path.join(dir, name);
      }
    }
  } catch {
    log.warn(`Directory not found: ${dir}`);
  }
}

// ─── Main ─────────────────────────────────────────────────────────────────────

async function main() {
  if (!GITHUB_RELEASE) { log.error('--github-release is required'); process.exit(1); }
  if (!GITHUB_TOKEN)   { log.error('GITHUB_TOKEN or --github-token is required'); process.exit(1); }

  console.log(`\n${c.bold}${c.blue}CMA Asset Delivery${c.reset}`);
  console.log(`  Mode    : ${c.cyan}${MODE}${c.reset}`);
  console.log(`  Release : ${c.cyan}${GITHUB_RELEASE}${c.reset}`);
  console.log(`  Repo    : ${c.dim}${REPO}${c.reset}`);
  if (DRY_RUN) console.log(`  ${c.yellow}DRY RUN — no uploads or registrations will be performed${c.reset}`);

  // ── 1. Find the GitHub release ──────────────────────────────────────────────
  log.header('GitHub release');
  log.info(`Looking up tag: ${GITHUB_RELEASE}`);
  let release;
  try {
    release = await getReleaseByTag(GITHUB_RELEASE);
  } catch (e) {
    log.error(`Release not found: ${e.message}`);
    process.exit(1);
  }
  log.ok(`"${release.name}" (id ${release.id})`);

  // ── 2. List existing assets (idempotency) ───────────────────────────────────
  const existing = await listReleaseAssets(release.id);
  const byName   = Object.fromEntries(existing.map(a => [a.name, a]));
  log.info(`${existing.length} asset(s) already on release`);

  // ── 3. Collect files to process ─────────────────────────────────────────────
  log.header('Collecting files');
  const files = []; // [{localPath, fileName, alreadyOnRelease?}]

  if (MODE === 'local') {
    if (!FILE_PATTERN) { log.error('--file-pattern is required in local mode'); process.exit(1); }
    for (const p of globFiles(FILE_PATTERN)) files.push({ localPath: p, fileName: path.basename(p) });
    if (files.length === 0) { log.warn(`No files matched: ${FILE_PATTERN}`); process.exit(0); }
    log.info(`${files.length} file(s) matched ${FILE_PATTERN}`);

  } else if (MODE === 'action') {
    if (!VERSION)           { log.error('--version is required in action mode'); process.exit(1); }
    if (!ARTIFACTORY_TOKEN) { log.error('ARTIFACTORY_ACCESS_TOKEN or --artifactory-token is required in action mode'); process.exit(1); }

    log.info(`Searching Artifactory for CMA ${VERSION} in stable paths…`);
    let results = [];
    try {
      results = await searchArtifactory(VERSION);
      log.info(`Found ${results.length} item(s) on Artifactory`);
    } catch (e) {
      log.warn(`Artifactory search failed: ${e.message}`);
    }

    const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'cma-deliver-'));

    if (results.length === 0) {
      // Fallback: re-process existing GitHub release assets (useful for re-registering on download.centreon.com)
      log.info(`No Artifactory results — re-downloading ${existing.length} existing release asset(s) for re-registration`);
      for (const asset of existing) {
        log.info(`Downloading existing asset: ${asset.name}`);
        const r = await fetch(asset.browser_download_url, {
          headers: { 'Authorization': `Bearer ${GITHUB_TOKEN}` },
        });
        if (!r.ok) { log.warn(`Could not download ${asset.name}: ${r.status}`); continue; }
        const dest = path.join(tmpDir, asset.name);
        fs.writeFileSync(dest, Buffer.from(await r.arrayBuffer()));
        files.push({ localPath: dest, fileName: asset.name, alreadyOnRelease: true });
      }
    } else {
      for (const { name, repo, path: artifactoryPath } of results) {
        // Filter by --distrib if provided
        if (DISTRIBS.length > 0) {
          const match = DISTRIBS.some(d =>
            d === 'windows' ? name.endsWith('.exe') : name.includes(d)
          );
          if (!match) { log.skip(`Filtered out: ${name}`); continue; }
        }
        try {
          const localPath = await downloadFromArtifactory(repo, artifactoryPath, name, tmpDir);
          files.push({ localPath, fileName: name });
        } catch (e) {
          log.error(`Failed to download ${name}: ${e.message}`);
        }
      }
    }

    log.info(`${files.length} file(s) ready to process`);

  } else {
    log.error(`Unknown mode: ${MODE} (expected local or action)`);
    process.exit(1);
  }

  // ── 4. Process each file ────────────────────────────────────────────────────
  log.header('Processing assets');
  let uploaded = 0, skipped = 0, failed = 0;

  for (const { localPath, fileName, alreadyOnRelease } of files) {
    log.file(fileName);

    const onRelease = alreadyOnRelease || !!byName[fileName];
    let downloadUrl;

    // GitHub release upload
    if (onRelease) {
      const asset = byName[fileName];
      downloadUrl = asset?.browser_download_url
        ?? `https://github.com/${REPO}/releases/download/${GITHUB_RELEASE}/${encodeURIComponent(fileName)}`;
      log.skip(`Already on release — skipping upload`);
      skipped++;
    } else if (DRY_RUN) {
      downloadUrl = `https://github.com/${REPO}/releases/download/${GITHUB_RELEASE}/${encodeURIComponent(fileName)}`;
      log.skip(`[dry-run] Would upload to GitHub release`);
      uploaded++;
    } else {
      log.upload(`Uploading to GitHub release…`);
      try {
        const data   = fs.readFileSync(localPath);
        const result = await uploadReleaseAsset(release.id, fileName, data);
        downloadUrl  = result.browser_download_url;
        log.ok(`Uploaded → ${downloadUrl}`);
        uploaded++;
      } catch (e) {
        log.error(`Upload failed: ${e.message}`);
        failed++;
        continue;
      }
    }

    // download.centreon.com registration
    if (DOWNLOAD_TOKEN) {
      if (onRelease && !FORCE_REREGISTER) {
        log.skip(`Asset already on release — skipping registration (use --force-reregister to override)`);
      } else {
        try {
          await registerOnDownloadCentreon(fileName, localPath, downloadUrl);
        } catch (e) {
          log.error(`download.centreon.com failed: ${e.message}`);
          failed++;
        }
      }
    }
  }

  // ── 5. Summary ──────────────────────────────────────────────────────────────
  console.log(`\n${c.bold}Summary${c.reset}`);
  console.log(`  Uploaded : ${c.green}${uploaded}${c.reset}`);
  console.log(`  Skipped  : ${c.dim}${skipped}${c.reset}`);
  console.log(`  Failed   : ${failed > 0 ? c.red : ''}${failed}${failed > 0 ? c.reset : ''}\n`);

  if (failed > 0) process.exit(1);
}

main().catch(e => {
  log.error(`Fatal: ${e.message}`);
  process.exit(1);
});

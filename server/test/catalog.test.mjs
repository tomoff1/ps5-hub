// SPDX-License-Identifier: GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import {DEFAULT_SOURCES, entry, https, legacy, tsv, validateSource} from '../src/model.mjs';
import {parsePspkgListing, readPublic, robotsAllows, synchronize} from '../src/connectors.mjs';

const source = DEFAULT_SOURCES.find(s => s.id === 'pspkg');
const page = 'https://pspkg.com/ps4/?page=1';
// Structure minimale représentative, sans texte commercial ni fichier de jeu.
const listing = `<div id="gamesContainer"><a class="hm-card" data-id="123"
 data-title="Exemple PS4" data-img="https://img.pspkg.com/test.webp"
 href="exemple-123.html"><div class="hm-card-title">Exemple PS4</div></a></div>
 <a href="?page=2">Suivant</a>`;

test('PSPKG : cartes, jaquettes et pagination, sans fabriquer de lien PKG', () => {
  const result = parsePspkgListing(listing, page, source);
  assert.equal(result.next, 'https://pspkg.com/ps4/?page=2');
  assert.equal(result.entries.length, 1);
  const e = result.entries[0];
  assert.equal(e.id, 'pspkg-ps4-123');
  assert.equal(e.platform, 'PS4');
  assert.equal(e.cover_url, 'https://img.pspkg.com/test.webp');
  assert.equal(e.access, 'verification');
  assert.equal(e.authorization, 'unconfirmed');
  assert.equal(e.url, 'https://pspkg.com/ps4/exemple-123.html');
  assert.equal(e.sha256, '');
  assert.equal(tsv([e]).trim().split('\n')[1].split('\t').length, 18);
});

test('une page de vérification ou une structure inconnue ne vide pas le catalogue', () => {
  assert.throws(() => parsePspkgListing('<html>Vérifiez votre navigateur</html>', page, source), /Aucune carte/);
});

test('robots : restrictions, exceptions, groupes spécifiques et fin de chemin', () => {
  const robots = 'User-agent: *\nDisallow: /api/\nDisallow: /ps4/download-\nAllow: /ps4/\nDisallow: /*.json$';
  assert.equal(robotsAllows(robots, page), true);
  assert.equal(robotsAllows(robots, 'https://pspkg.com/api/search.php'), false);
  assert.equal(robotsAllows(robots, 'https://pspkg.com/ps4/download-test.html'), false);
  assert.equal(robotsAllows(robots, 'https://pspkg.com/data.json'), false);
  assert.equal(robotsAllows(robots, 'https://pspkg.com/data.json/other'), true);
  assert.equal(robotsAllows('User-agent: *\nDisallow: /\nUser-agent: PS5-Hub\nAllow: /ps4/', page), true);
});

test('une route PSPKG exclue est refusée avant tout appel réseau', async () => {
  let calls = 0;
  await assert.rejects(readPublic('https://pspkg.com/api/search.php', source, async () => { calls++; }), /Route exclue/);
  assert.equal(calls, 0);
});

test('authentification, limitation et CAPTCHA restent des états explicites', async () => {
  for (const [status, state] of [[401, 'authentication'], [403, 'verification'], [429, 'rate-limited']]) {
    await assert.rejects(readPublic(page, source, async () => new Response('', {status})), e => e.state === state);
  }
  await assert.rejects(readPublic(page, source, async () => new Response('<script src="/challenge-platform/test"></script>')), e => e.state === 'verification');
});

test('un ETag de cache est lié à son URL, et aucune redirection n’est suivie', async () => {
  const old = {url: page, body: listing, etag: '"page-1"'};
  const cached = await readPublic(page, source, async (url, options) => {
    assert.equal(options.headers['If-None-Match'], '"page-1"');
    assert.equal(options.redirect, 'manual');
    return new Response(null, {status:304});
  }, old);
  assert.equal(cached, old);
  await readPublic('https://pspkg.com/ps4/?page=2', source, async (url, options) => {
    assert.equal(options.headers['If-None-Match'], undefined);
    return new Response(listing);
  }, old);
  await assert.rejects(readPublic(page, source, async () => new Response(null, {status:302, headers:{Location:'https://other.example/'}})), e => e.state === 'redirect');
});

test('le script passif JSD n’est pas confondu avec une vérification interactive', async () => {
  const html=listing+'<script src="/cdn-cgi/challenge-platform/scripts/jsd/main.js"></script>';
  const response=await readPublic(page,source,async()=>new Response(html));
  assert.equal(parsePspkgListing(response.body,page,source).entries.length,1);
});

test('sources et URL : validation des domaines et rejet des adresses locales', () => {
  assert.equal(validateSource(source).connector, 'pspkg');
  for (const url of ['http://example.com/', 'https://127.0.0.1/', 'https://[::1]/', 'https://user:pass@example.com/', 'https://console.local/']) {
    assert.throws(() => https(url));
  }
  assert.throws(() => validateSource({...source, url:'https://other.example/'}));
});

test('les téléchargements publics exigent une autorisation, un SHA et un format supporté', () => {
  const metadata = parsePspkgListing(listing, page, source).entries[0];
  assert.throws(() => entry({...metadata, access:'direct'}), /autorisation/);
  const authorized = {...metadata, access:'direct', authorization:'authorized', rights_url:'https://author.example/license',
    sha256:'a'.repeat(64), format:'PKG', url:'https://author.example/free.pkg'};
  assert.equal(entry(authorized).format, 'PKG');
  assert.throws(() => entry({...authorized, sha256:''}), /SHA-256/);
});

test('ancien TSV : compatibilité avec les archives communautaires', () => {
  const community = DEFAULT_SOURCES.find(s => s.id === 'community');
  const rows = legacy('example\tExemple\t1.0.0\thomebrew\thttps://github.com/author/example/releases/download/v1/app.zip\t'+'b'.repeat(64)+'\tProjet libre', community);
  assert.equal(rows[0].format, 'ZIP');
  assert.equal(rows[0].page_url, 'https://github.com/author/example');
});

test('une source désactivée ne déclenche aucune requête', async () => {
  const result = await synchronize({...source, enabled:false}, null, () => { throw new Error('appel interdit'); });
  assert.equal(result.state, 'disabled');
  assert.deepEqual(result.entries, []);
});

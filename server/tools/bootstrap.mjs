// SPDX-License-Identifier: GPL-3.0-or-later
// Prépare un import KV initial sans identifiant Cloudflare et sans fichier PKG.
import {mkdir, writeFile} from 'node:fs/promises';
import {dirname, resolve} from 'node:path';
import {syncCatalog} from '../src/worker.mjs';

const output = process.argv[2];
if (!output) throw new Error('Usage : node tools/bootstrap.mjs ../.local/cloudflare-bootstrap.json');
const values = new Map();
const HUB = {
  async get(key, type) { const value=values.get(key); return value===undefined?null:type==='json'?JSON.parse(value):value; },
  async put(key, value) { values.set(key, value); }
};
const snapshot = await syncCatalog({HUB});
await mkdir(dirname(resolve(output)), {recursive:true});
await writeFile(output, JSON.stringify([...values].map(([key,value]) => ({key,value}))), {mode:0o600});
console.log(JSON.stringify({count:snapshot.entries.length,sources:snapshot.sources.map(s=>({name:s.name,state:s.state,count:s.count,message:s.message}))},null,2));
if (!snapshot.entries.length) process.exitCode=1;

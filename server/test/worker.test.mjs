// SPDX-License-Identifier: GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import {route, syncCatalog} from '../src/worker.mjs';

class MemoryKV {
  data = new Map();
  async get(key, type) { const value=this.data.get(key); return value===undefined?null:type==='json'?JSON.parse(value):value; }
  async put(key, value) { this.data.set(key, value); }
}
const source = {id:'example',name:'Exemple',connector:'json',enabled:true,url:'https://author.example/feed.json',
  allowed_hosts:['author.example'],permission_url:'https://author.example/license'};
const e = {id:'example-game',title:'Jeu libre',version:'1.0.0',kind:'game',url:'https://author.example/free.pkg',sha256:'a'.repeat(64),
  description:'Exemple',source:'Exemple',platform:'PS5',format:'PKG',page_url:'https://author.example/',cover_url:'',
  access:'direct',authorization:'authorized',rights_url:'https://author.example/license',title_id:'',size:0,published:0};
const env = () => ({HUB:new MemoryKV(),ADMIN_TOKEN:'test-secret-'.repeat(5)});
async function setup() { const v=env(); await v.HUB.put('sources',JSON.stringify({revision:1,sources:[source]})); return v; }
const ok = async () => new Response(JSON.stringify([e]), {headers:{ETag:'"test"'}});

test('administration : authentification et origine vérifiées avant toute modification', async () => {
  const v=await setup();
  const request=(headers={})=>new Request('https://hub.example/api/admin/sources',{method:'PUT',headers,body:JSON.stringify({revision:1,sources:[{...source,enabled:false}]})});
  assert.equal((await route(request(),v)).status,401);
  assert.equal((await route(request({Authorization:'Bearer '+v.ADMIN_TOKEN,Origin:'https://other.example'}),v)).status,403);
  assert.equal((await v.HUB.get('sources','json')).revision,1);
  assert.equal((await route(request({Authorization:'Bearer '+v.ADMIN_TOKEN,Origin:'https://hub.example'}),v)).status,200);
  assert.equal((await v.HUB.get('sources','json')).revision,2);
});

test('administration : une révision périmée ne remplace pas la configuration', async () => {
  const v=await setup();
  const r=await route(new Request('https://hub.example/api/admin/sources',{method:'PUT',headers:{Authorization:'Bearer '+v.ADMIN_TOKEN},
    body:JSON.stringify({revision:0,sources:[source]})}),v);
  assert.equal(r.status,409);
});

test('synchronisation : une panne conserve seulement le cache de la source inchangée', async () => {
  const v=await setup();
  assert.equal((await syncCatalog(v,ok)).entries.length,1);
  const outage=async()=>new Response('',{status:403});
  const cached=await syncCatalog(v,outage);
  assert.equal(cached.entries.length,1);
  assert.equal(cached.sources[0].state,'verification');
  await v.HUB.put('sources',JSON.stringify({revision:2,sources:[{...source,url:'https://author.example/new.json'}]}));
  assert.equal((await syncCatalog(v,outage)).entries.length,0);
});

test('désactiver la dernière source retire les fiches du catalogue publié', async () => {
  const v=await setup(); await syncCatalog(v,ok);
  await v.HUB.put('sources',JSON.stringify({revision:2,sources:[{...source,enabled:false}]}));
  const empty=await syncCatalog(v,()=>{throw new Error('Aucune requête attendue');});
  assert.deepEqual(empty.entries,[]);
  assert.equal((await v.HUB.get('catalog','json')).entries.length,0);
});

test('pagination et recherche : les ETags dépendent aussi de la requête', async () => {
  const v=await setup(); await syncCatalog(v,ok);
  const first=await route(new Request('https://hub.example/api/catalog?q=libre&pageSize=1'),v);
  const tag=first.headers.get('ETag');
  assert.equal((await first.json()).total,1);
  const same=await route(new Request('https://hub.example/api/catalog?q=libre&pageSize=1',{headers:{'If-None-Match':tag}}),v);
  assert.equal(same.status,304);
  const other=await route(new Request('https://hub.example/api/catalog?q=absent&pageSize=1',{headers:{'If-None-Match':tag}}),v);
  assert.equal(other.status,200);
  assert.equal((await other.json()).total,0);
  assert.notEqual(other.headers.get('ETag'),tag);
  assert.equal((await route(new Request('https://hub.example/api/catalog?page=0'),v)).status,400);
});

test('le flux natif expose la provenance sans secret administrateur', async () => {
  const v=await setup(); await syncCatalog(v,ok);
  const r=await route(new Request('https://hub.example/api/native/catalog.tsv'),v),text=await r.text();
  assert.match(text,/#SOURCE\texample\tExemple\tready/);
  assert.equal(text.includes(v.ADMIN_TOKEN),false);
  assert.equal(text.split('\n').find(line=>line.startsWith('example-game\t')).split('\t').length,18);
});

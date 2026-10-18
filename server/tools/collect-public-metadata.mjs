// SPDX-License-Identifier: GPL-3.0-or-later
// Import initial explicite des listes publiques, sans API ni page download-.
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve,dirname} from 'node:path';
import {DEFAULT_SOURCES,MAX_ENTRIES} from '../src/model.mjs';
import {readPublic,robotsAllows,parsePspkgListing} from '../src/connectors.mjs';
import {syncCatalog} from '../src/worker.mjs';

if (process.argv[2]!=='--all') throw new Error('Utilisez --all pour demander un import initial complet des métadonnées publiques.');
const folder=resolve('../.local');await mkdir(folder,{recursive:true});
const source=DEFAULT_SOURCES.find(s=>s.id==='pspkg');
const progressFile=folder+'/pspkg-public-progress.json';
let progress={entries:[],next:{PS4:'https://pspkg.com/ps4/?page=1',PS5:'https://pspkg.com/ps5/?page=1'}};
try { progress=JSON.parse(await readFile(progressFile,'utf8')); } catch(e) { if(e.code!=='ENOENT')throw e; }
const entries=new Map(progress.entries.map(e=>[e.id,e]));
const robots=await readPublic('https://pspkg.com/robots.txt',source);
for(const platform of ['PS4','PS5']){
  for(let pages=0;progress.next[platform];pages++){
    if(pages>=350)throw new Error('Limite de 350 pages par plateforme atteinte ; reprise enregistrée');
    const url=progress.next[platform];
    if(!robotsAllows(robots.body,url))throw new Error('robots.txt refuse la page : collecte arrêtée');
    const r=await readPublic(url,source),result=parsePspkgListing(r.body,url,source);
    for(const item of result.entries)entries.set(item.id,item);
    if(entries.size>MAX_ENTRIES)throw new Error('Limite de fiches atteinte');
    progress.next[platform]=result.next;
    progress.entries=[...entries.values()];
    await writeFile(progressFile,JSON.stringify(progress));
    console.log(`${platform} page ${new URL(url).searchParams.get('page')||1} : ${entries.size} références collectées`);
    await new Promise(r=>setTimeout(r,1000));
  }
}
// Prépare un import KV complet, avec l'empreinte de configuration du Worker.
const fingerprint=[...new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(JSON.stringify(source))))].map(n=>n.toString(16).padStart(2,'0')).join('');
const state={entries:[...entries.values()],fingerprint,cursors:{PS4:1,PS5:1},state:'metadata-only',complete:true,
  updated_at:new Date().toISOString(),message:'Listes publiques PS4/PS5 parcourues. Références uniquement ; aucun lien PKG extrait.'};
const kv=new Map([['source:pspkg',JSON.stringify(state)]]);
const HUB={async get(k,t){const v=kv.get(k);return v===undefined?null:t==='json'?JSON.parse(v):v;},async put(k,v){kv.set(k,v);}};
const snapshot=await syncCatalog({HUB},async(url,options)=>{
  // Les premières listes publiques sont rafraîchies par le connecteur normal ;
  // la collection complète reste dans son cache et n'est pas refaite ici.
  return fetch(url,options);
});
await writeFile(folder+'/cloudflare-full-catalog.json',JSON.stringify([...kv].map(([key,value])=>({key,value}))));
console.log(`Export KV prêt : ${snapshot.entries.length} fiches dont ${entries.size} références PSPKG.`);

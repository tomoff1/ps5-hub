// SPDX-License-Identifier: GPL-3.0-or-later
import {DEFAULT_SOURCES,MAX_ENTRIES,entry,tsv,validateSource} from './model.mjs';
import {synchronize} from './connectors.mjs';

const json=(data,status=200,headers={})=>new Response(JSON.stringify(data),{status,headers:{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store',...headers}});
const encoder=new TextEncoder();
async function hash(s){return [...new Uint8Array(await crypto.subtle.digest('SHA-256',encoder.encode(s)))].map(n=>n.toString(16).padStart(2,'0')).join('');}
async function sameSecret(a,b){const x=await hash(a),y=await hash(b);let diff=0;for(let i=0;i<x.length;i++)diff|=x.charCodeAt(i)^y.charCodeAt(i);return diff===0;}
const saved=async(env,key,fallback)=>await env.HUB.get(key,'json')??fallback;
async function body(request,max=3*1024*1024){
  if(Number(request.headers.get('Content-Length')||0)>max)throw new Error('Requête trop grande');
  const reader=request.body?.getReader();if(!reader)throw new Error('Corps requis');let parts=[],length=0;
  while(true){const {done,value}=await reader.read();if(done)break;length+=value.length;if(length>max){await reader.cancel();throw new Error('Requête trop grande');}parts.push(value);}
  const bytes=new Uint8Array(length);let at=0;for(const p of parts){bytes.set(p,at);at+=p.length;}
  return JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes));
}
async function config(env){return saved(env,'sources',{revision:0,sources:DEFAULT_SOURCES});}

export async function syncCatalog(env,fetcher=fetch){
  const cfg=await config(env), now=new Date().toISOString(),statuses=[],all=new Map();
  // Une mise à jour concurrente de la configuration impose une nouvelle synchronisation.
  for(const source of cfg.sources){
    const key='source:'+source.id,stored=await saved(env,key,null);
    const fingerprint=await hash(JSON.stringify(source));
    // Une URL ou une autorisation modifiée ne réutilise pas l'ancien export.
    const previous=stored?.fingerprint===fingerprint?stored:null;
    let result;
    try{
      result=await synchronize(source,previous,fetcher);
      result={...result,updated_at:now,source_revision:cfg.revision,fingerprint};
      if((await config(env)).revision!==cfg.revision)throw new Error('Configuration modifiée ; relancer la synchronisation');
      await env.HUB.put(key,JSON.stringify(result));
    }catch(error){
      result={...previous,entries:source.enabled?(previous?.entries||[]):[],state:error.state||'error',message:String(error.message).slice(0,300),updated_at:previous?.updated_at||null};
    }
    statuses.push({id:source.id,name:source.name,state:result.state,message:result.message,updated_at:result.updated_at,
      count:result.entries?.length||0,complete:result.complete??false,cursors:result.cursors||null});
    for(const e of result.entries||[]){
      if(all.has(e.id))throw new Error('Conflit d’identifiant entre sources : publication refusée');
      all.set(e.id,e);
    }
  }
  const personal=await saved(env,'curated',[]);
  for(const raw of personal){const e=entry(raw);all.set(e.id,e);}
  if(all.size>MAX_ENTRIES)throw new Error('Limite globale de 10000 fiches dépassée ; copie précédente conservée');
  // Une suppression volontaire doit pouvoir retirer toutes les fiches.
  // Les sources inchangées conservent leur propre cache en cas de panne.
  if((await config(env)).revision!==cfg.revision)throw new Error('Configuration modifiée ; publication différée');
  const entries=[...all.values()],digest=await hash(JSON.stringify({entries,statuses}));
  const snapshot={schema:2,version:digest,updated_at:now,entries,sources:statuses};
  await env.HUB.put('catalog',JSON.stringify(snapshot));
  await env.HUB.put('job',JSON.stringify({state:'complete',at:now,version:digest,count:entries.length}));
  return snapshot;
}
export async function route(request,env,ctx={waitUntil:()=>{}}){
  const url=new URL(request.url),path=url.pathname;
  if(!path.startsWith('/api/'))return env.ASSETS.fetch(request);
  if(!env.HUB)return json({error:'Stockage Cloudflare KV non configuré'},503);
  if(path.startsWith('/api/admin/')){
    if(!env.ADMIN_TOKEN||env.ADMIN_TOKEN.length<32)return json({error:'ADMIN_TOKEN doit être configuré avec au moins 32 caractères'},503);
    const token=request.headers.get('Authorization')||'';
    if(!token.startsWith('Bearer ')||!await sameSecret(token.slice(7),env.ADMIN_TOKEN))return json({error:'Connexion administrateur requise'},401);
    const origin=request.headers.get('Origin');
    if(origin&&origin!==url.origin)return json({error:'Origine refusée'},403);
    if(path==='/api/admin/state'&&request.method==='GET'){
      const cfg=await config(env),catalog=await saved(env,'catalog',null),job=await saved(env,'job',{state:'idle'});
      return json({...cfg,catalog:catalog?{version:catalog.version,updated_at:catalog.updated_at,count:catalog.entries.length,sources:catalog.sources}:null,
        curated:await saved(env,'curated',[]),job});
    }
    if(path==='/api/admin/sources'&&request.method==='PUT'){
      const data=await body(request),old=await config(env);
      if(data.revision!==old.revision)return json({error:'Configuration modifiée : rechargez avant de sauvegarder'},409);
      if(!Array.isArray(data.sources)||!data.sources.length||data.sources.length>12)throw new Error('1 à 12 sources requises');
      const sources=data.sources.map(validateSource);
      if(new Set(sources.map(s=>s.id)).size!==sources.length)throw new Error('Identifiants sources en doublon');
      const next={revision:old.revision+1,sources};await env.HUB.put('sources',JSON.stringify(next));return json(next);
    }
    if(path==='/api/admin/entries'&&request.method==='PUT'){
      const data=await body(request);if(!Array.isArray(data.entries)||data.entries.length>MAX_ENTRIES)throw new Error('Liste de fiches invalide');
      const entries=data.entries.map(entry);if(new Set(entries.map(e=>e.id)).size!==entries.length)throw new Error('Doublons');
      await env.HUB.put('curated',JSON.stringify(entries));return json({saved:entries.length,message:'Fiches enregistrées ; synchronisez pour publier'});
    }
    if(path==='/api/admin/sync'&&request.method==='POST'){
      const last=await saved(env,'job',null);
      if(last?.at&&Date.now()-Date.parse(last.at)<60000)return json({error:'Attendez une minute entre deux synchronisations'},429);
      await env.HUB.put('job',JSON.stringify({state:'running',at:new Date().toISOString()}));
      ctx.waitUntil(syncCatalog(env).catch(async error=>env.HUB.put('job',JSON.stringify({state:'error',at:new Date().toISOString(),message:String(error.message).slice(0,300)}))));
      return json({state:'running',message:'Synchronisation demandée ; son résultat est consultable dans le tableau de bord'},202);
    }
    return json({error:'Route ou méthode inconnue'},404);
  }
  if(request.method!=='GET'&&request.method!=='HEAD')return json({error:'Méthode refusée'},405);
  const snapshot=await saved(env,'catalog',null);
  if(!snapshot)return json({error:'Catalogue non initialisé ; lancer une synchronisation depuis l’administration'},503);
  const etag='"'+await hash(snapshot.version+'|'+path+url.search)+'"';
  const headers={'ETag':etag,'Cache-Control':'public, max-age=60','X-Catalog-Version':snapshot.version};
  if(request.headers.get('If-None-Match')===etag)return new Response(null,{status:304,headers});
  if(path==='/api/native/catalog.tsv')return new Response(request.method==='HEAD'?null:tsv(snapshot.entries,snapshot.sources),{headers:{...headers,'Content-Type':'text/tab-separated-values; charset=utf-8'}});
  if(path==='/api/sources')return json({version:snapshot.version,sources:snapshot.sources},200,headers);
  if(path==='/api/catalog'){
    const page=Number(url.searchParams.get('page')||1),pageSize=Number(url.searchParams.get('pageSize')||40);
    if(!Number.isInteger(page)||page<1||!Number.isInteger(pageSize)||pageSize<1||pageSize>100)return json({error:'Pagination invalide'},400);
    const q=(url.searchParams.get('q')||'').normalize('NFD').replace(/\p{Diacritic}/gu,'').toLocaleLowerCase('fr').split(/\s+/).filter(Boolean);
    let entries=snapshot.entries.filter(e=>(!url.searchParams.get('platform')||e.platform===url.searchParams.get('platform'))&&
      (!url.searchParams.get('source')||e.source===url.searchParams.get('source'))&&q.every(word=>[e.title,e.description,e.source,e.title_id].join(' ').normalize('NFD').replace(/\p{Diacritic}/gu,'').toLocaleLowerCase('fr').includes(word)));
    if(url.searchParams.get('sort')==='newest')entries.sort((a,b)=>b.published-a.published||a.title.localeCompare(b.title));
    const total=entries.length;entries=entries.slice((page-1)*pageSize,page*pageSize);
    return json({schema:2,version:snapshot.version,updated_at:snapshot.updated_at,total,page,pageSize,entries,sources:snapshot.sources},200,headers);
  }
  if(path.startsWith('/api/entries/')){
    const e=snapshot.entries.find(e=>e.id===path.slice('/api/entries/'.length));return e?json(e,200,headers):json({error:'Fiche introuvable'},404);
  }
  return json({error:'Route inconnue'},404);
}
export default {
  async fetch(request,env,ctx){try{return await route(request,env,ctx);}catch(error){return json({error:String(error.message).slice(0,300)},400);}},
  async scheduled(event,env,ctx){ctx.waitUntil(syncCatalog(env).catch(async error=>env.HUB.put('job',JSON.stringify({state:'error',at:new Date().toISOString(),message:String(error.message).slice(0,300)}))));}
};

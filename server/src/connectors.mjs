// SPDX-License-Identifier: GPL-3.0-or-later
import {parseHTML} from 'linkedom';
import {entry,https,legacy,MAX_ENTRIES,text} from './model.mjs';
const UA='PS5-Hub/0.4 (+https://github.com/tomoff1/ps5-hub)';

export class SourceError extends Error { constructor(state,message){super(message);this.state=state;} }
export async function readPublic(url, source, fetcher=fetch, old=null) {
  https(url,source.allowed_hosts);
  // Jamais de cookies, d'API privée ou de récupération de liens protégés.
  if(source.connector==='pspkg' && /\/(api|common|login|signup|profile|jsonfile|bsss|akira)\/|\/(?:ps4|ps5)\/download-/i.test(new URL(url).pathname))
    throw new SourceError('restricted','Route exclue par la politique PSPKG');
  const headers={'User-Agent':UA,'Accept':'application/json,text/html,text/plain'};
  if(old?.etag&&old.url===url)headers['If-None-Match']=old.etag;
  const response=await fetcher(url,{headers,redirect:'manual',signal:AbortSignal.timeout(8000)});
  if(response.status===304&&old?.body&&old.url===url)return old;
  if([401,403,429].includes(response.status))throw new SourceError(response.status===401?'authentication':response.status===429?'rate-limited':'verification',`La source répond HTTP ${response.status} ; accès arrêté`);
  if(response.status>=300&&response.status<400)throw new SourceError('redirect','Redirection de source non suivie ; vérifier son URL');
  if(!response.ok)throw new SourceError('unavailable',`Source indisponible (HTTP ${response.status})`);
  const reader=response.body.getReader();let bytes=0,parts=[];
  while(true){const {done,value}=await reader.read();if(done)break;bytes+=value.length;if(bytes>3*1024*1024){await reader.cancel();throw new Error('Réponse limitée à 3 Mio');}parts.push(value);}
  const buffer=new Uint8Array(bytes);let offset=0;for(const p of parts){buffer.set(p,offset);offset+=p.length;}
  const body=new TextDecoder('utf-8',{fatal:true}).decode(buffer);
  // Le script passif Cloudflare JSD accompagne aussi des pages publiques 200.
  // Il n'est jamais exécuté ici. Les écrans interactifs restent bloquants.
  if(/cf-chl-|challenge-platform\/(?!scripts\/jsd\/)|g-recaptcha|h-captcha/i.test(body))throw new SourceError('verification','Vérification interactive de la source requise');
  return {url,body,etag:response.headers.get('ETag')||'',at:Date.now()};
}

export function robotsAllows(robots,url) {
  const pathname=new URL(url).pathname+new URL(url).search;
  let agents=[],rules=[],groupRules=false;
  const groups=[];
  for(const raw of robots.split(/\r?\n/)){
    const line=raw.split('#')[0].trim(), colon=line.indexOf(':');if(colon<0)continue;
    const key=line.slice(0,colon).toLowerCase(),value=line.slice(colon+1).trim();
    if(key==='user-agent'){
      if(groupRules){groups.push({agents,rules});agents=[];rules=[];groupRules=false;}
      agents.push(value.toLowerCase());
    }else if(['allow','disallow'].includes(key)&&agents.length){groupRules=true;if(value)rules.push({allow:key==='allow',path:value});}
  }
  groups.push({agents,rules});
  const exact=groups.filter(g=>g.agents.some(a=>a!=='*'&&'ps5-hub'.includes(a)));
  const chosen=exact.length?exact:groups.filter(g=>g.agents.includes('*'));
  let match=null;
  for(const rule of chosen.flatMap(g=>g.rules)){
    const re='^'+rule.path.split('*').map(p=>p.replace(/[.+?^${}()|[\]\\]/g,'\\$&')).join('.*').replace(/\\\$$/,'$');
    if(new RegExp(re).test(pathname)&&(!match||rule.path.length>match.path.length||rule.path.length===match.path.length&&rule.allow))match=rule;
  }
  return !match||match.allow;
}

export function parsePspkgListing(html,base,source) {
  const {document}=parseHTML(html), origin=new URL(base).origin;
  const platform=new URL(base).pathname.startsWith('/ps4/')?'PS4':'PS5';
  const rows=[];
  for(const card of document.querySelectorAll('#gamesContainer a.hm-card[data-id]')){
    const remoteId=card.getAttribute('data-id');if(!/^\d{1,10}$/.test(remoteId||''))continue;
    const page=https(new URL(card.getAttribute('href'),base).href,source.allowed_hosts);
    if(new URL(page).origin!==origin || !new URL(page).pathname.startsWith('/'+platform.toLowerCase()+'/'))continue;
    rows.push(entry({id:`pspkg-${platform.toLowerCase()}-${remoteId}`,title:text(card.getAttribute('data-title')||card.querySelector('.hm-card-title')?.textContent,120),
      kind:'game',version:'0.0.0',url:page,sha256:'',description:`Fiche ${platform} publiée sur PSPKG. Version et format à consulter dans la fiche source.`,
      source:source.name,platform,format:'REFERENCE',page_url:page,cover_url:https(card.getAttribute('data-img')||card.querySelector('img')?.getAttribute('src'),source.allowed_hosts),
      access:'verification',authorization:'unconfirmed',rights_url:'',title_id:'',size:0,published:0}));
  }
  if(!rows.length)throw new SourceError('structure-changed','Aucune carte publique reconnue ; ancienne copie conservée');
  const current=Number(new URL(base).searchParams.get('page')||1);
  const next=[...document.querySelectorAll('a[href]')].map(a=>new URL(a.getAttribute('href'),base)).find(u=>u.origin===origin&&u.pathname===new URL(base).pathname&&Number(u.searchParams.get('page'))===current+1);
  return {entries:rows,next:next?.href||null};
}

export function parsePspkgDetail(html,url,source,baseEntry) {
  const {document}=parseHTML(html);
  const info=document.querySelector('.game-info,.game-information,.info-grid,.game-info-grid');
  const metadata=info?.textContent||'';
  const version=metadata.match(/(?:Update Version|Version)\s*:?\s*v?(\d{1,3}(?:\.\d{1,3}){1,2})/i)?.[1];
  const titleId=metadata.match(/\b(?:CUSA|PPSA)\d{5}\b/)?.[0]||'';
  const format=metadata.match(/\b(PKG|FPKG|PFS|LZ4)\b/i)?.[1]?.toUpperCase();
  const e={...baseEntry,title_id:titleId};
  if(version)e.version=version.split('.').concat('0').slice(0,3).join('.');
  if(format)e.format=format==='FPKG'?'PKG':format;
  // La prose commerciale et les commentaires ne sont pas recopiés. Les
  // champs inconnus restent inconnus et ne deviennent jamais une URL PKG.
  e.description=`Fiche ${e.platform} PSPKG${titleId?' · '+titleId:''}${version?' · version '+version:''}${format?' · '+format:''}. Téléchargement direct non fourni par ce connecteur.`;
  return entry(e);
}

const connectors={
  async tsv(source,previous,fetcher){const r=await readPublic(source.url,source,fetcher,previous?.http);return {entries:legacy(r.body,source),http:r,state:'ready',message:'Flux TSV autorisé',complete:true};},
  async json(source,previous,fetcher){
    const r=await readPublic(source.url,source,fetcher,previous?.http),data=JSON.parse(r.body),items=Array.isArray(data)?data:data.entries;
    if(!Array.isArray(items)||items.length>MAX_ENTRIES)throw new Error('Export JSON invalide');
    const entries=items.map(v=>entry({...v,source:source.name}));
    return {entries,http:r,state:'ready',message:'Export JSON autorisé',complete:true};
  },
  async pspkg(source,previous,fetcher){
    const robots=await readPublic('https://pspkg.com/robots.txt',source,fetcher);
    const cursors=structuredClone(previous?.cursors||{PS4:1,PS5:1}), old=new Map((previous?.entries||[]).map(e=>[e.id,e]));
    const pageCache=previous?.pageCache||{};
    for(const platform of ['PS4','PS5']){
      const url=`https://pspkg.com/${platform.toLowerCase()}/?page=${cursors[platform]}`;
      if(!robotsAllows(robots.body,url))throw new SourceError('restricted','Exploration interdite par robots.txt');
      const r=await readPublic(url,source,fetcher,pageCache[platform]);pageCache[platform]=r;
      const parsed=parsePspkgListing(r.body,url,source);
      for(const item of parsed.entries)old.set(item.id,item);
      cursors[platform]=parsed.next?cursors[platform]+1:1;
    }
    return {entries:[...old.values()].slice(0,MAX_ENTRIES),cursors,pageCache,state:'metadata-only',complete:false,
      message:'Métadonnées publiques uniquement. Pages de téléchargement et API internes exclues ; aucune autorisation PKG déduite.'};
  }
};
export async function synchronize(source,previous,fetcher=fetch) {
  if(!source.enabled)return {entries:[],state:'disabled',message:'Source désactivée',complete:true};
  const connector=connectors[source.connector];if(!connector)throw new Error('Connecteur non enregistré');
  return connector(source,previous,fetcher);
}

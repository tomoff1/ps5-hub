// SPDX-License-Identifier: GPL-3.0-or-later
export const MAX_ENTRIES = 10000;
export const FIELDS = ['id','title','version','kind','url','sha256','description','source','platform',
  'format','page_url','cover_url','access','authorization','rights_url','title_id','size','published'];

export function text(value, max=400) {
  const s = String(value ?? '').replace(/\s+/g,' ').trim();
  if (/[\x00-\x1f\x7f]/.test(s) || new TextEncoder().encode(s).length>max) throw new Error('Champ invalide ou trop long');
  return s;
}
export function https(value, allowedHosts) {
  const s=text(value,2048), u=new URL(s);
  if(u.protocol!=='https:' || u.username || u.password || u.port && u.port!=='443' ||
    /[\\<>"\s]/.test(s) || !u.hostname.includes('.') || u.hostname.endsWith('.local') ||
    /^\d+(?:\.\d+){3}$/.test(u.hostname) || u.hostname.startsWith('[')) throw new Error('URL HTTPS publique requise');
  if(allowedHosts && !allowedHosts.includes(u.hostname)) throw new Error('Hôte absent de la liste autorisée');
  return u.href;
}
export function entry(input) {
  const e=Object.fromEntries(FIELDS.map(k=>[k,input[k]??'']));
  e.id=text(e.id,64); if(!/^[a-z0-9-]+$/.test(e.id))throw new Error('Identifiant invalide');
  e.title=text(e.title,120); if(!e.title)throw new Error('Titre manquant');
  e.description=text(e.description,400);e.source=text(e.source||'Source autorisée',80);
  e.version=text(e.version||'0.0.0',11);if(!/^\d{1,3}\.\d{1,3}\.\d{1,3}$/.test(e.version))throw new Error('Version invalide');
  if(!['game','homebrew','pkg','link'].includes(e.kind))throw new Error('Type invalide');
  e.url=https(e.url);
  for(const k of ['page_url','cover_url','rights_url'])if(e[k])e[k]=https(e[k]);
  if(!['PS4','PS5','UNKNOWN'].includes(e.platform))throw new Error('Plateforme invalide');
  if(!['PKG','ZIP','PFS','LZ4','REFERENCE'].includes(e.format))throw new Error('Format invalide');
  if(!['direct','reference','authentication','verification','unavailable'].includes(e.access))throw new Error('Accès invalide');
  if(!['authorized','unconfirmed'].includes(e.authorization))throw new Error('Autorisation invalide');
  e.sha256=text(e.sha256,64);if(e.sha256 && !/^[a-f0-9]{64}$/.test(e.sha256))throw new Error('SHA-256 invalide');
  e.title_id=text(e.title_id,32);
  for(const k of ['size','published']){ e[k]=Number(e[k]||0); if(!Number.isSafeInteger(e[k])||e[k]<0)throw new Error('Nombre invalide'); }
  if(e.size>250*1024**3)throw new Error('Fichier supérieur à 250 Gio');
  if(e.access==='direct' && (e.authorization!=='authorized'||!e.rights_url||!e.sha256||!['PKG','ZIP'].includes(e.format)))
    throw new Error('Un fichier direct exige une autorisation référencée, un SHA-256 et un format PKG/ZIP');
  return e;
}
export function tsv(entries, sources=[]) {
  return ['# PS5 Hub catalogue v2', ...sources.map(s=>['#SOURCE',text(s.id,64),text(s.name,80),text(s.state,48),text(s.message||'',300)].join('\t')),
    ...entries.map(e=>FIELDS.map(k=>e[k]??'').join('\t'))].join('\n')+'\n';
}
export function legacy(textData, source) {
  return textData.split(/\r?\n/).filter(s=>s&&!s.startsWith('#')).map(line=>{
    const row=line.split('\t');if(row.length!==7 && row.length!==18)throw new Error('TSV invalide');
    if(row.length===18)return entry(Object.fromEntries(FIELDS.map((k,i)=>[k,row[i]])));
    const [id,title,version,kind,url,sha256,description]=row;
    const direct=kind!=='link'&&!!sha256;
    return entry({id,title,version,kind,url,sha256,description,source:source.name,platform:'PS5',format:direct?'ZIP':'REFERENCE',
      page_url:url.replace(/(https:\/\/github\.com\/[^/]+\/[^/]+)\/.*/, '$1'),cover_url:`https://raw.githubusercontent.com/tomoff1/ps5-hub/main/assets/covers/${id}.hubart`,
      access:direct?'direct':'reference',authorization:'authorized',rights_url:source.permission_url});
  });
}
export function validateSource(value) {
  const s=structuredClone(value);
  s.id=text(s.id,64);if(!/^[a-z0-9-]+$/.test(s.id))throw new Error('Identifiant source invalide');
  s.name=text(s.name,80);if(!s.name)throw new Error('Nom requis');
  if(!['pspkg','json','tsv'].includes(s.connector))throw new Error('Connecteur inconnu');
  s.url=https(s.url);s.enabled=!!s.enabled;
  if(!Array.isArray(s.allowed_hosts)||!s.allowed_hosts.length||s.allowed_hosts.length>12)throw new Error('Hôtes autorisés requis');
  s.allowed_hosts=s.allowed_hosts.map(h=>new URL(https('https://'+h)).hostname);
  https(s.url,s.allowed_hosts);
  if(s.connector==='pspkg' && new URL(s.url).origin!=='https://pspkg.com')throw new Error('Origine PSPKG invalide');
  if(s.connector!=='pspkg')s.permission_url=https(s.permission_url);
  return s;
}
export const DEFAULT_SOURCES = [
  {id:'community',name:'Communauté PS5',connector:'tsv',enabled:true,url:'https://raw.githubusercontent.com/tomoff1/ps5-hub/main/shared/catalog.tsv',
    allowed_hosts:['raw.githubusercontent.com'],permission_url:'https://github.com/blackbearreloaded/ps5-homebrew-catalog/blob/main/LICENSE'},
  {id:'pspkg',name:'PSPKG',connector:'pspkg',enabled:true,url:'https://pspkg.com/',allowed_hosts:['pspkg.com','img.pspkg.com','image.api.playstation.com'],
    policy:'public-metadata-only',permission_url:'https://pspkg.com/robots.txt'}
];

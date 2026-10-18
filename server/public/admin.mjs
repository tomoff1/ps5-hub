let token='',state=null,page=1,total=0,timer=null;
const $=id=>document.getElementById(id),notice=s=>$('notice').textContent=s;
const labels={'metadata-only':'Métadonnées uniquement',ready:'Synchronisé',disabled:'Désactivé',error:'Erreur',verification:'Vérification requise',authentication:'Connexion requise','rate-limited':'Limite de requêtes',direct:'Fichier direct autorisé',reference:'Référence',unavailable:'Indisponible'};
async function api(path,options={}){
  const r=await fetch(path,{...options,headers:{...(path.startsWith('/api/admin/')?{'Authorization':'Bearer '+token}:{}),'Content-Type':'application/json',...options.headers}});
  const d=await r.json();if(!r.ok)throw new Error(d.error||'Erreur du service');return d;
}
function logout(){token='';state=null;clearTimeout(timer);$('token').value='';$('dashboard').hidden=true;$('login').hidden=false;$('logout').hidden=true;notice('Déconnecté');}
function element(tag,text,className){const e=document.createElement(tag);e.textContent=text;if(className)e.className=className;return e;}
async function load(){
  state=await api('/api/admin/state');$('sources').replaceChildren();
  for(const s of state.sources){
    const status=state.catalog?.sources.find(x=>x.id===s.id),card=element('article','','source');
    card.append(element('h3',s.name),element('span',s.enabled?(labels[status?.state]||'En attente de synchronisation'):'Désactivé','badge'));
    card.append(element('p',status?.message||s.url),element('p',`${status?.count||0} fiches · ${status?.complete?'instantané complet':'synchronisation progressive'}`));
    const toggle=element('button',s.enabled?'Désactiver':'Activer');toggle.onclick=()=>act(async()=>{
      const sources=state.sources.map(x=>x.id===s.id?{...x,enabled:!x.enabled}:x);
      await api('/api/admin/sources',{method:'PUT',body:JSON.stringify({revision:state.revision,sources})});await load();notice('Configuration enregistrée. Synchronisez pour publier.');
    });card.append(toggle);$('sources').append(card);
  }
  $('summary').textContent=state.catalog?`${state.catalog.count} fiches · publié le ${new Date(state.catalog.updated_at).toLocaleString('fr-FR')} · version ${state.catalog.version.slice(0,12)}`:'Aucun catalogue publié. Lancez la première synchronisation.';
  $('entries').value=JSON.stringify(state.curated,null,2);
  if(state.job?.state==='running'){notice('Synchronisation en cours…');timer=setTimeout(()=>act(load),2500);}
  else if(state.job?.state==='error')notice(state.job.message);
  await preview();
}
async function preview(){
  if(!state?.catalog)return;
  const d=await api('/api/catalog?'+new URLSearchParams({page,pageSize:30,q:$('search').value}));total=d.total;
  $('preview').replaceChildren();for(const e of d.entries){const tr=document.createElement('tr');for(const value of [e.title,e.platform,e.source,labels[e.access]||e.access])tr.append(element('td',value));$('preview').append(tr);}
  $('page-label').textContent=`Page ${page} · ${total} résultats`;$('prev').disabled=page===1;$('next').disabled=page*30>=total;
}
async function act(fn){try{await fn();}catch(e){notice(e.message);}}
$('login-form').onsubmit=e=>{e.preventDefault();act(async()=>{token=$('token').value;await load();$('token').value='';$('login').hidden=true;$('dashboard').hidden=false;$('logout').hidden=false;notice('Connexion établie');});};
$('logout').onclick=logout;
$('sync').onclick=()=>act(async()=>{await api('/api/admin/sync',{method:'POST',body:'{}'});notice('Synchronisation demandée…');clearTimeout(timer);timer=setTimeout(()=>act(load),1500);});
$('source-form').onsubmit=e=>{e.preventDefault();act(async()=>{
  const data=Object.fromEntries(new FormData(e.target)),source={...data,allowed_hosts:data.hosts.split(',').map(s=>s.trim()).filter(Boolean),enabled:true};delete source.hosts;
  await api('/api/admin/sources',{method:'PUT',body:JSON.stringify({revision:state.revision,sources:[...state.sources,source]})});e.target.reset();await load();notice('Source enregistrée. Lancez la synchronisation.');
});};
$('save-entries').onclick=()=>act(async()=>{const entries=JSON.parse($('entries').value);await api('/api/admin/entries',{method:'PUT',body:JSON.stringify({entries})});notice('Fiches enregistrées. Synchronisez pour les publier.');});
$('search-form').onsubmit=e=>{e.preventDefault();page=1;act(preview);};
$('prev').onclick=()=>{page=Math.max(1,page-1);act(preview);};$('next').onclick=()=>{page++;act(preview);};

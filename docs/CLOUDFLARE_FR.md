# Catalogue distant et administration Cloudflare

Le dossier `server` contient un Worker et un tableau de bord en français. Il
publie des métadonnées, jamais les fichiers PKG eux-mêmes. Cette étape est
indépendante du remplacement de l’exécutable : le client 00.004.000 lit ce flux
à dix-huit colonnes ; les anciens clients 00.003.000 restent sur le flux GitHub
à sept colonnes tant que le titre n’a pas été mis à jour.

## Configuration et caches sur D:

Depuis le dépôt Windows, avec Node.js 22 ou ultérieur installé :

```powershell
$env:npm_config_cache = Join-Path (Get-Location) '.local\npm-cache'
npm.cmd ci --prefix server
.\Start-HubServer.ps1 -Action test
.\Start-HubServer.ps1 -Action build
```

Le lanceur conserve les temporaires, le cache npm et les journaux Wrangler dans
`.local` du projet. Les dépendances sont dans `server/node_modules`, le paquet
de contrôle dans `server/build`. Ces dossiers sont exclus de Git. Le lanceur
restaure les variables d'environnement du processus appelant à sa sortie.
Les identifiants OAuth restent dans l'emplacement utilisateur normal de Wrangler.

## Déployer dans son propre compte

Connectez Wrangler avec `wrangler login --device` depuis `server` (la commande
est accessible avec `npx.cmd --no-install wrangler`). Créez un espace KV avec
`wrangler kv namespace create HUB --binding HUB --update-config`. Utilisez votre
propre identifiant KV dans `server/wrangler.jsonc` : celui du dépôt appartient au
déploiement public PS5 Hub. Les autorisations nécessaires concernent le compte,
l'utilisateur, les Workers et KV ; les autres services Cloudflare ne sont pas
utilisés par ce projet.

Déployez avec `Start-HubServer.ps1 -Action deploy`, puis configurez un secret
`ADMIN_TOKEN` aléatoire d'au moins 32 caractères avec `wrangler secret put
ADMIN_TOKEN`. Ne placez ce secret ni dans le dépôt, ni dans le catalogue, ni dans
l'application PS5. Le domaine `workers.dev` retourné par Wrangler peut nécessiter
un délai d'activation DNS/TLS lors de sa première création. Ne désactivez pas la
vérification des certificats pour le contourner.

Le propriétaire du déploiement préparé dans cette session trouve sa clé dans
`.local/cloudflare-admin-token.txt`. Cette clé n'est pas fournie aux utilisateurs
de la console. Elle sert à se connecter au tableau de bord depuis un téléphone
ou un ordinateur ; elle reste en mémoire dans l'onglet et disparaît à sa fermeture.

La première synchronisation peut être lancée depuis le tableau de bord. Pour
préparer un premier instantané avant que le domaine soit disponible :

```powershell
cd server
node tools/bootstrap.mjs ../.local/cloudflare-bootstrap.json
npx.cmd --no-install wrangler kv bulk put ../.local/cloudflare-bootstrap.json --binding HUB --remote
```

Ce bootstrap convient à un nouvel espace KV. Ne l'utilisez pas pour écraser un
catalogue administré existant : il construit un instantané avec les sources par
défaut et ne récupère pas les réglages distants ni les fiches éditoriales.

## Sources et limites vérifiées

- Le connecteur TSV conserve la compatibilité avec les 92 fiches du catalogue
  communautaire PS5 Hub existant.
- Le connecteur PSPKG lit `robots.txt` avant les listes publiques `/ps4/` et
  `/ps5/`. Il reconnaît leurs cartes HTML, titres, plateformes, identifiants de
  fiche, URL de page et URL de jaquette. Il actualise la première page de chaque plateforme et une page ancienne par
  plateforme à chaque synchronisation (deux à quatre pages au total), avec cache ETag associé à chaque URL et curseurs persistés.
- Les API internes et pages `download-` exclues ne sont pas interrogées. Aucune API
  publique officielle documentée n'a été identifiée. Une page publique accessible
  ne constitue pas une autorisation de redistribuer le jeu correspondant.
- Les fiches PSPKG portent `metadata-only` pour la source, `verification` pour
  l'accès et `unconfirmed` pour l'autorisation. Aucun lien PKG n'en est extrait.
  Le script passif Cloudflare JSD présent sur une liste accessible n'est pas
  exécuté ; les véritables écrans interactifs, CAPTCHA et réponses 401/403/429
  arrêtent la récupération sans tentative de résolution.
- Les versions, identifiants de titre et formats absents des listes restent
  inconnus. Le lecteur de détail est préparé mais son enrichissement automatique
  n'est pas activé. Les longues descriptions commerciales ne sont pas recopiées.
- Les sources JSON et TSV peuvent être ajoutées avec leurs domaines autorisés et
  une référence de licence. Un fichier public direct exige une autorisation
  référencée, un SHA-256 et un format PKG ou ZIP. Le Worker n'héberge ni ne sert
  d'intermédiaire aux téléchargements de jeux.
- Une panne conserve le cache d'une source inchangée. Modifier son URL ou sa
  référence d'autorisation invalide ce cache. Désactiver toutes les sources peut
  publier un catalogue vide pour retirer effectivement leurs fiches.
- La synchronisation horaire est configurée à la minute 17. La limite globale est
  de 10000 fiches. Les références disparues de PSPKG ne sont pas encore purgées
  automatiquement. L’import initial du 18 octobre a parcouru 274 pages PS4 et 38 pages PS5 : 5571 identifiants uniques ont été conservés. Le catalogue public totalise 5663 fiches avec les projets communautaires. Ces nombres décrivent les listes accessibles pendant le relevé, sans garantie d’exhaustivité de tout le site.

## API et cohérence

`GET /api/catalog` accepte `page`, `pageSize` (1 à 100), `q`, `platform`,
`source` (nom affiché) et `sort=newest`. Les dates manquantes valent zéro :
l'ordre « nouveautés » n'invente pas de date de publication PSPKG.
`GET /api/sources` décrit l'état de chaque source.
`GET /api/native/catalog.tsv` fournit l'instantané natif v2 avec ses lignes
`#SOURCE`. Les réponses possèdent un ETag associé à la version et à la requête.

L'administration utilise un en-tête Bearer et refuse les origines étrangères.
Elle permet d'activer/désactiver et d'ajouter des sources, d'importer les fiches
éditoriales JSON et de demander une synchronisation avec suivi de son résultat.
L'import des fiches remplace la liste éditoriale : sauvegardez-la avant modification.
La gestion des sources existantes reste partielle : leurs détails se modifient
par l'API `PUT /api/admin/sources`, avec la révision courante, pas encore par un
formulaire d'édition dédié.

Cloudflare KV a une cohérence éventuelle : les utilisateurs de différents centres
de données peuvent voir des versions différentes pendant la propagation. Les
révisions détectent certaines modifications concurrentes mais ne constituent pas
une transaction globale. Évitez plusieurs éditeurs ou synchronisations simultanés.
Un verrou transactionnel nécessiterait un composant supplémentaire.

Les tests Node couvrent la structure des cartes, la pagination, les règles robots,
les restrictions d'accès, les ETags, les autorisations de fichiers directs, le
cache par source, les suppressions et les contrôles administrateur. Les tests HTTPS locaux exercent aussi le vrai téléchargeur C++ et ses reprises.
Ils ne valident ni un téléchargement PKG sur PS5, ni etaHEN, ni le firmware 13.60.

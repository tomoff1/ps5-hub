# PS5 Hub 0.6 — fonctions natives et limites

Cette version réutilise le titre C++20/OpenGL, le SDK public 0.42, le runtime
du boilerplate, l’adaptateur libcurl PS5 et les composants DualSense existants.
Ce n’est pas un site Web. Les QR sont retirés de l’interface.

## Ouvrir une fiche sur la console

Dans une fiche, **Triangle : Ouvrir sur PS5**. Pour une référence sans fichier
direct autorisé, Croix affiche la cause du blocage et propose le filtre
**Téléchargeables**, sans lancer le navigateur. Seul Triangle ouvre la page.
Le code appelle `sceSystemServiceLaunchWebBrowser(uri, nullptr)`, avec l’ABI
à deux arguments du [sample du SDK](https://github.com/ps5-payload-dev/sdk/blob/master/samples/browser/main.c).
Le symbole existe dans le stub `libSceSystemService.so` du SDK utilisé.
Le service utilisateur est déjà initialisé par la couche DualSense du titre.

Il s’agit du **navigateur système**, pas d’un navigateur intégré dessiné par le
Hub. Un retour zéro signifie que le système accepte la demande ; il ne prouve
pas que la page s’affiche. Les erreurs retournées apparaissent en hexadécimal.
Ouverture, navigation DualSense, retour au Hub, TLS, connexions, redirections
et vérifications des sites doivent être testés sur la console 13.60. Aucun
WebView compatible avec ce contexte n’a été établi. Il n’y a pas de navigateur
de secours fictif lorsque l’API refuse la demande.

Le navigateur garde sa propre session. Aucun mécanisme documenté utilisé ici
ne transmet ses cookies ou les liens sélectionnés à libcurl. Une connexion Web
réussie ne débloque donc pas automatiquement un téléchargement dans le Hub.
Les protections du site restent en place. L’application ne récupère pas de
liens commerciaux cachés et ne réécrit pas les téléchargements PSPKG.

## Télécharger sans téléphone

Il faut une entrée avec URL HTTPS directe, autorisation de diffusion confirmée
ou contenu personnel, et SHA-256 fourni par une source de confiance. Le
catalogue partagé peut publier ces entrées ; Triangle dans Jeux, Homebrew ou
Applications permet aussi de saisir un lien personnel à la manette.
Sans empreinte, le lien peut être conservé mais ne peut pas être téléchargé.

Croix sur une fiche directe autorisée ajoute immédiatement le contenu à une
**file séquentielle**, sans ouvrir de navigateur ni demander une nouvelle
confirmation pour le téléchargement seul. Le mode combiné d’installation
exige une confirmation distincte. Dans
**Téléchargements** :

- Croix sur un travail en attente ou en cours : pause ;
- Croix sur un travail en pause, en échec ou annulé : reprendre ou recommencer ;
- Carré : annuler et supprimer uniquement `.part` et `.resume` de ce travail ;
- Croix sur un PKG terminé : proposer l’envoi à etaHEN après confirmation ;
- Triangle : afficher le diagnostic complet ;
- Croix sur une demande acceptée : proposer de supprimer les fichiers du Hub
  après confirmation manuelle que l’installation est terminée dans le système.

La barre affiche octets reçus, taille lorsqu’elle est connue, vitesse en Mo/s
(décimal, 1 Mo = 1 000 000 octets) et durée
restante estimée. Pendant la vérification d’un préfixe, la vitesse peut être
nulle. La reprise utilise Range/If-Range, l’ETag et une identité de contenu ; si
le serveur ignore Range, le transfert repart de zéro. Les données finales
ne sont publiées qu’après SHA-256 et inspection de format pour les PKG.
Le HTTPS vérifie les certificats ; les réponses HTML et erreurs d’accès sont
refusées plutôt que traitées comme des PKG.

`/download0/hub/transfers.tsv` conserve la file et l’historique de 200 contenus.
Les URL personnelles y restent privées. Après redémarrage, les travaux
inachevés sont restaurés en pause et nécessitent Croix pour repartir. Un
journal invalide est conservé et bloque les nouveaux ajouts jusqu’à réparation.
Les messages sont aussi affichés dans le Hub et les résultats consignés dans
ses journaux. Aucun service de téléchargement en arrière-plan du système n’est
installé : garder le titre ouvert ; une suspension par le navigateur ou le
système n’est pas validée sur console. L’historique n’est pas illimité et son
purgeur graphique n’est pas encore implémenté.

Le plafond logiciel est de 250 Gio. L’espace accessible est vérifié avant un
transfert de taille connue, en tenant compte d’une copie partielle ; un quota
ou un disque plein provoque une erreur. Le quota réel de `/download0` sur 13.60
reste à mesurer. Une taille inconnue ne permet pas de garantir l’espace avant
la réception. Le catalogue PSPKG actuel fournit des références, pas des URL
PKG directes autorisées ; il n’ajoute donc pas de bouton Télécharger à ces fiches.

## Installer avec etaHEN

Le Hub vérifie au démarrage et depuis **Paramètres > Détecter etaHEN DPIv2** :

- `GET http://127.0.0.1:12800/`, sans proxy ni redirection, avec réponse bornée ;
- identité attendue `etaHEN DPIv2`, formulaire `/upload` et champ `file` ;
- à défaut, connexion TCP sans données sur 9090 pour signaler un port ouvert,
  sans l’assimiler à un service identifié ni lui envoyer de commande.

Le protocole est tiré du [code etaHEN DirectPKGInstaller](https://github.com/etaHEN/etaHEN/blob/dafa13b562ddb137a4b4a97b9aaa287c0c57cc9c/Source%20Code/util/source/DirectPKGInstaller.cpp),
vérifié également sur la branche publique lors du développement. Le port 9090
legacy reçoit du JSON brut ; il ne fournit pas ce transfert de fichier local.
Un autre outil utilisant 9090 ou 12800 n’est pas automatiquement compatible.
Le Hub ne démarre ni ne reconfigure etaHEN.

Après confirmation explicite : inspection bornée CNT/FIH, cohérence plateforme
et identifiant lorsqu’ils sont fournis, nouveau SHA-256 du fichier local,
contrôle de l’espace pour sa copie de staging dans `/user/data/tmp`, puis
nouvelle identification du service. Si ce répertoire n’est pas accessible pour
la vérification, l’envoi est refusé. L’upload multipart `/upload`, champ `file`,
est diffusé depuis le disque sans charger le PKG entier en RAM. L’URL distante
n’est pas envoyée à l’installateur.

Le contrôle d’espace porte sur la copie temporaire. La taille décompressée,
les quotas finaux et le firmware minimum ne sont pas déduits de l’en-tête
CNT/FIH. L’inspection ne valide ni signatures, ni clés, ni DRM, ni licence,
ni jouabilité. Les erreurs système finales restent gérées par etaHEN.

Seule la réponse HTTP 200 commençant par
`SUCCESS: PKG installation started from file:` marque une **demande acceptée**.
Elle ne marque jamais « installé » ou « jouable ». Le nettoyage du Hub exige
une confirmation manuelle de réussite dans les notifications système ; il ne
supprime pas les temporaires partagés d’etaHEN et ne désinstalle aucun jeu. Un refus affiche son code
etaHEN lorsqu’il est présent. Après interruption ou réponse perdue, l’état est
**inconnu**, sans réessai automatique. Vérifier les notifications système avant
toute nouvelle demande. Aucun endpoint de suivi de fin d’installation n’est
inventé. Les copies de staging etaHEN ne sont pas effacées automatiquement.

## Validation restante

Les tests PC utilisent des fichiers synthétiques et des serveurs locaux.
La compilation PS5 ne constitue pas un test matériel. Voir le
[protocole 13.60](COMPATIBILITE_13_60.md) et le [rapport 0.6](VALIDATION_ALPHA6.md).
Le remplacement automatique du binaire n’est pas implémenté : le premier
passage à cette version nécessite encore de copier le nouveau dossier natif.

## Mode Télécharger et installer

Dans une fiche de PKG direct autorisé, Carré propose ce mode uniquement si
DPIv2 est identifié. Une confirmation autorise le téléchargement puis l’envoi
après SHA-256, inspection et contrôle d’espace de staging. Aucun envoi après
échec de téléchargement, état d’installation inconnu ou réponse perdue. Après
redémarrage du titre, l’autorisation d’installation automatique n’est pas
rejouée : reprendre le téléchargement puis confirmer l’installation manuellement.

## Diagnostic des liens qui ne démarrent pas

L’inventaire du catalogue livré ne contient aucun PKG direct déclaré autorisé.
Les entrées PSPKG sont des références, pas des fichiers. Le filtre
Téléchargeables contient notamment des archives homebrew ; il ne promet pas
une collection de jeux PKG. Un administrateur peut publier une source autorisée
avec URL directe, format, taille et SHA-256, ou vous pouvez ajouter votre lien
personnel depuis le clavier du Hub. Une extension `.pkg` dans l’URL n’est pas
une preuve de format ; un lien sans cette extension peut être un vrai fichier.

Le transfert natif suit au plus cinq redirections HTTP vers HTTPS. Il
n’exécute ni JavaScript publicitaire, ni pop-up, ni redirection HTML/meta-refresh.
Si une vérification ou une session est nécessaire, il s’arrête et l’explique.
Les vérifications TLS restent actives. Le Hub inspecte MIME et premiers octets,
refuse HTML/XHTML même avec un faux type MIME et vérifie le préfixe CNT/FIH
pour un PKG. Il vérifie ensuite le SHA-256 et inspecte les tables CNT/FIH dans les limites documentées.

| Diagnostic | Interprétation et action |
|---|---|
| HTTP 401 | Connexion requise ; la session du navigateur n’est pas partagée. |
| HTTP 403 | Accès refusé ; droits, vérification ou expiration possibles. Le code seul ne permet pas de les départager. |
| HTTP 404 / 410 | Fichier absent ou retiré ; actualiser la source ou utiliser un autre lien autorisé. |
| HTTP 429 | Limitation ; respecter Retry-After numérique lorsqu’il est fourni, aucun réessai automatique agressif. |
| HTML_PAGE | Page intermédiaire reçue ; utiliser une URL directe autorisée. |
| NOT_PKG | Le corps ne commence pas par une signature PKG prise en charge. |
| CURL_47 | Trop de redirections ; transfert arrêté. |
| CURL_60 | Certificat refusé ; vérifier date de la console, identité et chaîne du serveur. |
| CURL_77 | Fichier CA système absent/illisible ; vérifier le montage du titre. |
| NO_SPACE / STORAGE | Espace ou écriture refusés ; vérifier quota et permissions, errno dans le diagnostic. |

`network.log` et `network.log.1` restent dans le stockage privé du Hub et sont
bornés par rotation à environ 256 Kio chacun. Ils enregistrent date, identifiant,
chaîne de statuts HTTP, nombre de redirections, code libcurl, résultat de
vérification TLS, errno et octets reçus. Ni URL, ni jeton, ni cookie, ni corps
HTTP ou tampon d’erreur brut n’y est journalisé. Les URL personnelles restent
néanmoins dans `personal.tsv` / `transfers.tsv` : ne pas publier ces fichiers.
Une tentative peut n’avoir aucun statut HTTP si elle échoue avant réception
DNS/TLS ou avant la création du fichier. La console cible n’a pas fourni de
journal pour reproduire un échec précis ; les diagnostics ne sont pas une
preuve de la cause d’une tentative antérieure.

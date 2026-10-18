# Validation de l’alpha 0.4

Relevé du 18 octobre 2026. Cette version reste expérimentale : aucun test réel
de 0.4 sur PS5 13.60 n’a été effectué. Le retour matériel antérieur concerne
uniquement l’affichage et le démarrage de 0.1.

## Vérifications réalisées

- 436 tests C++ sur PC, dont le catalogue à dix-huit colonnes, la provenance,
  les états d’accès, les formats d’images et des conteneurs CNT/FIH synthétiques.
  La suite a aussi été exécutée avec les 5663 fiches fournies dans le paquet.
- 8 tests HTTPS locaux du téléchargeur C++, avec AddressSanitizer et
  UndefinedBehaviorSanitizer : transfert vérifié, reprise 206, serveur ignorant
  Range, mauvaise plage, coupure réseau suivie d’une reprise, mauvais SHA-256,
  réponses d’authentification/HTML et taille inattendue. Aucun jeu ni console
  n’est utilisé par ces tests.
- 20 tests Node sur les connecteurs et le Worker : pagination réelle, liens
  localisés, images absentes, robots.txt, absence de contournement, cache ETag,
  autorisation des fichiers directs, authentification et retrait de sources.
- Service Cloudflare publié : réponses HTTPS 200 sur le tableau de bord,
  le catalogue paginé, le filtre PS5 et le flux natif complet de 2136475 octets.
  Le catalogue compte 5663 fiches, dont 850 marquées PS5 (92 communautaires et
  758 références PSPKG) et 4813 références PS4.
- 274 pages publiques PS4 et 38 pages PS5 parcourues avec pause entre requêtes,
  contrôle robots et déduplication. Les fiches ne contiennent pas de lien PKG
  commercial. Les URL de jaquettes sont des références distantes.
- Contrôle du paquet : signature FSELF, fichiers requis, licences et SHA-256
  par `tools/hub-package.py`. Les artefacts de préversion sont publiés seulement
  après réussite du workflow GitHub pour leur commit source.

La CI exécute aussi les 11 tests Python existants du bot et du transfert FTP,
la compilation PS5 et le rendu hors écran du vrai C++ avec Mesa. Les captures
du pipeline sont des rendus PC et ne valident pas les pilotes de la console.

## Limites restantes

Le contenu des listes publiques n’est pas une garantie d’exhaustivité de tout
PSPKG. Les versions, descriptions commerciales et identifiants de titre absents
des listes ne sont pas inventés. Les suppressions de références PSPKG ne sont
pas encore purgées automatiquement. Les jaquettes s’affichent après récupération
réseau ; seules les anciennes icônes communautaires sont fournies hors ligne.

La reprise nécessite que la source accepte Range ou permette de recommencer.
Le SHA-256 reste obligatoire et les réponses HTML sont refusées. Le plafond
logiciel de 250 Gio ne valide pas le quota disponible dans `/download0/hub`.
L’inspection CNT/FIH vérifie les limites des tables, la plateforme et l’identifiant
lorsqu’il est fourni ; elle ne vérifie ni la signature Sony, ni la déchiffrabilité,
ni la compatibilité du jeu avec 13.60.

L’installation système des PKG depuis le hub et la mise à jour automatique de
l’exécutable ne sont pas implémentées. Les services de la console sondée n’ont
pas répondu ; aucun transfert, lancement ou appel d’installation n’a été effectué.
La livraison demandée est un ZIP à copier par USB, titre fermé, selon
[le guide d’installation](INSTALLATION_FR.md).

L’actualisation du catalogue fonctionne au démarrage puis toutes les quinze
minutes pendant que le titre est ouvert. Elle ne remplace pas le binaire PS5.
La détection et l’installation etaHEN, DualSense réel, cadence, audio et TLS du
runtime restent à valider sur la configuration matérielle exacte.

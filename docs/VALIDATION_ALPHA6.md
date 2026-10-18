# Validation — alpha 0.6

Cette version réutilise le client natif C++ et le runtime existants. Le retour
utilisateur « l’application fonctionne » ne valide pas les nouvelles fonctions
0.6 sur PS5 13.60. Aucun essai matériel de la 0.6 n’a été effectué ici.

La suite comporte 456 tests C++, 22 scénarios HTTPS et 9 scénarios DPIv2
simulés. Les contrôles automatisés du commit de publication sont disponibles
dans son workflow GitHub Actions. Les scénarios ajoutés couvrent :

- redirection HTTPS vers un vrai fichier, sans exposer son jeton dans le diagnostic ;
- redirection vers une page HTML, faux Content-Type, XHTML et HTML fragmenté ;
- erreurs 404/410, limitation 429 avec Retry-After, boucle de redirections et
  refus d’une redirection vers HTTP ;
- espace insuffisant détecté avant le corps, erreur de création du stockage
  avec errno et certificat CA absent sans désactiver la vérification TLS ;
- reprise depuis la fiche sans duplication de l’historique, refus du mode
  combiné sans installateur identifié, absence de rejeu automatique d’une
  installation après redémarrage ;
- disparition du stockage après mise en file : état d’échec récupérable et retrait du consentement à l’installation ;
- nettoyage uniquement après demande acceptée et confirmation manuelle,
  préservation des fichiers d’un autre contenu et refus d’un état inconnu.

Les tests C++ et réseau locaux utilisent AddressSanitizer et
UndefinedBehaviorSanitizer. Les tests HTTPS exécutent le vrai téléchargeur,
et les tests DPIv2 existants exécutent le vrai connecteur face à un simulateur.
Le rendu hors écran utilise les composants et shaders C++ sous Mesa.

La compilation et le contrôle du FSELF ne prouvent pas le fonctionnement sur
console. Restent à tester : droits et quota `/download0`, accès à l’espace de
staging `/user/data/tmp`, TLS système, services etaHEN réellement présents,
installation finale et lancement. Une réponse HTTP 403 ne permet pas de
distinguer à elle seule droits manquants, vérification ou expiration ; le Hub
affiche ces possibilités sans inventer une cause précise.

Le catalogue fourni ne contient aucun PKG direct déclaré autorisé : PSPKG y
figure comme source de références. Le filtre Téléchargeables propose des
fichiers directs autorisés disponibles, notamment des archives homebrew ; il
ne garantit pas une collection de jeux au format PKG. Aucun lien caché ou
protégé n’est extrait. Fournir un PKG direct autorisé et son SHA-256 reste
nécessaire pour tester un téléchargement/installation de ce format.

La fin d’installation n’est pas exposée par le protocole HTTP utilisé ici.
Le nettoyage dépend donc d’une confirmation humaine dans le Hub après
vérification des notifications système, jamais d’une simple demande acceptée.

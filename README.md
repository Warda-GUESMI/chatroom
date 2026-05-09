# ChatRoom — Serveur de Chat Multi-Rooms en C

Projet de programmation systèmes et réseaux implémentant un serveur de messagerie en temps réel, développé en langage C. L'architecture repose sur une gestion explicite de la concurrence, une communication par sockets TCP/IP et un modèle producteur/consommateur pour le traitement des requêtes clients.

---

## Architecture technique

### Synchronisation

La cohérence des données partagées est assurée par un ensemble de mécanismes de synchronisation fine :

- Mutex dédiés par ressource : `UserManager`, `RoomManager`, et chaque instance `User` disposent de leur propre verrou, limitant la contention et les risques d'interblocage.
- Variables de condition sur la file de tâches (`Queue`) pour la coordination entre threads producteurs et consommateurs.

### Modèle de threads

Le serveur utilise un thread pool fixe de huit workers. Les connexions entrantes sont enregistrées dans une file de tâches partagée ; les workers les consomment de façon concurrente, évitant la création de threads à la demande.

### Communication réseau

- Protocole TCP/IP avec sockets bloquantes côté serveur.
- Client multi-thread : un thread dédié à l'envoi, un autre à la réception, permettant une interaction simultanée avec le serveur.

---

## Compilation et lancement

Le projet se compile via `make` depuis la racine du dépôt. Le serveur s'exécute en précisant le port d'écoute ; chaque client se connecte en fournissant l'adresse IP et le port du serveur.

---

## Commandes disponibles

| Commande | Description |
|---|---|
| `/nick <pseudo>` | Définir ou modifier son pseudo (unicité garantie) |
| `/join <salle>` | Rejoindre une salle de discussion |
| `/msg <texte>` | Envoyer un message dans la salle courante |
| `/pm <utilisateur> <texte>` | Envoyer un message privé à un utilisateur |
| `/list` | Lister les salles actives et leurs membres |
| `/quit` | Se déconnecter proprement du serveur |

---

## Corrections apportées

La version actuelle corrige plusieurs défauts de concurrence et de robustesse identifiés dans l'implémentation initiale.

| # | Problème | Correction |
|---|---|---|
| 1 | Race condition sur les champs `nick` et `current_room` de `User` | Ajout d'un mutex `user->lock` par utilisateur |
| 2 | Deadlock potentiel dans `CMD_JOIN` (`global_lock` + `room->lock`) | Lecture des champs sous `u->lock`, ordre de verrouillage documenté |
| 3 | Appel à `write()` en tenant `room->lock`, bloquant la salle sur client lent | Copie des descripteurs sous verrou, écriture après relâchement |
| 4 | Buffer `response` de 1024 octets insuffisant pour `/list` avec 64 utilisateurs | Buffer agrandi à 4096 octets |
| 5 | Utilisation de `room_get_or_create` à la place de `room_find` pour `MSG` | Ajout de `room_find()`, qui ne crée pas de salle fantôme |
| 6 | Pseudo non unique, rendant `/pm` ambigu | Vérification d'unicité dans `CMD_NICK` |
| 7 | Écritures partielles et interruptions `EINTR` ignorées | Fonction `write_all()` avec boucle de réessai |
| 8 | Pseudo par défaut basé sur le descripteur de fichier, réutilisable | Compteur atomique monotone |
| 9 | `SIGPIPE` non ignoré, provoquant un crash sur déconnexion client | `signal(SIGPIPE, SIG_IGN)` ajouté côté serveur et client |

---

## Analyse et débogage

L'outil **Helgrind** (inclus dans Valgrind) permet de détecter les races conditions et interblocages résiduels. La commande `ss` permet de vérifier l'état du port d'écoute.

---

## Structure du projet

```
chatroom/
├── client/          # Code source du client
├── server/          # Code source du serveur (thread pool, socket, queue)
├── shared/          # Structures partagées (message, protocole, room, user)
├── Makefile

```

---

## Technologies

- Langage : C (C11)
- Concurrence : POSIX Threads (pthreads)
- Réseau : POSIX Sockets (TCP/IP)
- Outils : GCC, Make, Valgrind / Helgrind

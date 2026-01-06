# Documentation Technique - Serveur de Chat Multiclients

**Projet:** Mini-projet USSI07 - Systèmes et Architecture des Machines  
**Auteurs:** Lucas MARROT et Kyliann CLARET-LAVAL  
**Date:** Janvier 2026

---

## Table des matières

1. [Vue d'ensemble du projet](#vue-densemble)
2. [Concepts fondamentaux](#concepts-fondamentaux)
3. [Architecture globale](#architecture-globale)
4. [Analyse détaillée du CLIENT](#client)
5. [Analyse détaillée du SERVEUR](#serveur)
6. [Flux de communication](#flux-de-communication)
7. [Gestion de la concurrence](#gestion-de-la-concurrence)
8. [Exécution et compilation](#exécution-et-compilation)

---

## Vue d'ensemble du projet {#vue-densemble}

### Qu'est-ce que ce projet fait ?

Ce projet implémente un **serveur de chat multiclients** permettant à plusieurs utilisateurs de:

- Se connecter simultanément à un serveur
- Discuter en temps réel dans des **canaux de discussion (channels)**
- Consulter l'historique des messages d'un canal
- Changer de canal facilement
- Avoir une persistance des messages (sauvegardés dans des fichiers)

### Caractéristiques principales

| Aspect              | Description                            |
| ------------------- | -------------------------------------- |
| **Protocole**       | TCP/IP (sockets réseau)                |
| **Architecture**    | Client-serveur avec multithreading     |
| **Gestion clients** | Un thread dédié par client             |
| **Stockage**        | Fichiers texte (un par canal)          |
| **Ports**           | Port 12345 (défini par `#define PORT`) |
| **Adresse serveur** | 127.0.0.1 (localhost)                  |

---

## Concepts Fondamentaux {#concepts-fondamentaux}

Avant de plonger dans le code, comprenons les concepts clés.

### 1. Les Sockets Réseau

**En termes simples:** Une socket est un "point de connexion" pour communiquer sur un réseau.

**Analogie:** Pensez à une prise téléphonique. Un socket est comme:

- **Client:** Vous appelez quelqu'un (vous initiez une connexion)
- **Serveur:** Quelqu'un qui attend vos appels (écoute les connexions)

**Type utilisé:** `SOCK_STREAM` (connexion TCP)

- Garantit que les données arrivent dans l'ordre
- Garantit qu'elles arrivent complètes
- Établit une connexion avant de transférer les données

### 2. Les Threads (Fils d'exécution)

**En termes simples:** Un thread est comme une "tâche parallèle" qui s'exécute en même temps que d'autres.

**Pourquoi c'est utile:**

- Sans threads: Le serveur traite 1 client, puis attend qu'il se déconnecte avant de traiter un autre
- Avec threads: Le serveur traite **tous les clients simultanément**

**Analogie:**
Un serveur est comme un restaurant:

- **Sans threads:** Une seule serveuse pour tous les clients (elle prend la commande du client 1, sert le client 1, puis passe au client 2)
- **Avec threads:** Une serveuse par table (chacune s'occupe de son client en même temps)

```
Serveur SANS threads (séquentiel):
Client 1 → Service 1 → Service 2 → Service 3 → Déconnexion
                                                    ↓
                                          Client 2 → Service 1 → ...

Serveur AVEC threads (parallèle):
Thread 1: Client 1 → Service 1 → Service 2 → Service 3 → Déconnexion
Thread 2:          Client 2 → Service 1 → Service 2 → Service 3 → Déconnexion
Thread 3:                    Client 3 → Service 1 → ...
```

**Dans notre projet:**

```c
pthread_t thread_id;
if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0)
```

- Pour chaque nouveau client, on crée un thread qui exécute la fonction `handle_client`
- Cette fonction gère TOUT cet utilisateur (réception, envoi, changement de canal)

### 3. Les Mutex (Verrous de synchronisation)

**En termes simples:** Un mutex est un "verrou" pour empêcher deux threads d'accéder à la même ressource en même temps.

**Problème résolu:**
Imaginons deux clients écrivant dans le même fichier en même temps:

```
Thread 1: "Écrit: Bonjour"  ← Au même moment
Thread 2: "Écrit: Salut"    ← que Thread 2

Résultat possible dans le fichier: "BonjourSalut" OU "SalutBonjour" OU pire!
```

**Solution - Utiliser un mutex:**

```
Thread 1: Verrouille le mutex → Écrit "Bonjour" → Déverrouille
                                    ↓
                          Thread 2 attend ici
Thread 2:                                      Verrouille → Écrit "Salut" → Déverrouille
```

**Dans notre projet:**

```c
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // Créer le verrou

pthread_mutex_lock(&mutex);      // AVANT d'accéder aux données partagées
// ... modifier les données ...
pthread_mutex_unlock(&mutex);    // APRÈS avoir modifié
```

Les données partagées à protéger:

- `channels[]` - Liste des canaux
- `channel_count` - Nombre de canaux

### 4. La Sérialisation (struct sockaddr_in)

**En termes simples:** Une structure qui contient les informations de connexion réseau.

```c
struct sockaddr_in {
    sin_family;      // AF_INET = IPv4
    sin_addr;        // Adresse IP (ex: 127.0.0.1)
    sin_port;        // Port (ex: 12345)
}
```

---

## Architecture Globale {#architecture-globale}

### Vue d'ensemble du système

```
┌─────────────────────────────────────────────────────────────────┐
│                      RÉSEAU INTERNET (TCP/IP)                   │
└────────────┬────────────────────────────────┬────────────────────┘
             │                                │
      ┌──────▼──────┐                  ┌──────▼──────┐
      │   CLIENT 1  │                  │   CLIENT 2  │
      │             │                  │             │
      │ Socket TCP  │                  │ Socket TCP  │
      │ (port aléa) │                  │ (port aléa) │
      └──────┬──────┘                  └──────┬──────┘
             │                                │
             │        Connexion TCP           │
             │      (Port 12345)              │
             │                                │
             └────────────────┬───────────────┘
                              │
                       ┌──────▼──────────┐
                       │     SERVEUR     │
                       │                 │
                       │ Socket serveur  │
                       │ (port 12345)    │
                       │                 │
                       │ Thread 1 ──── CLIENT 1
                       │ Thread 2 ──── CLIENT 2
                       │ Thread 3 ──── CLIENT 3
                       │ ...            ...
                       │                 │
                       │   [Channels]    │
                       │   [Mutex]       │
                       │                 │
                       │ [Fichiers]      │
                       │ Stockage disque │
                       └─────────────────┘
```

### Estructura des données principales

#### 1. Structure Channel (dans server.c)

```c
typedef struct {
    char name[50];              // Nom du canal (ex: "général", "musique")
    int clients[MAX_CLIENTS];   // IDs des sockets des clients connectés
    int client_count;           // Nombre actuel de clients
} Channel;
```

**Exemple:**

```
Channel "général":
  - name: "général"
  - clients: [15, 18, 22]  (3 sockets de clients)
  - client_count: 3

Channel "musique":
  - name: "musique"
  - clients: [20]  (1 socket de client)
  - client_count: 1
```

#### 2. Variables globales du serveur

```c
Channel channels[MAX_CHANNELS];        // Tableau de 100 canaux max
int channel_count = 0;                 // Nombre de canaux créés
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // Verrou global
```

---

## Analyse Détaillée du CLIENT {#client}

### Vue d'ensemble du client

Le client gère:

1. **Connexion** au serveur
2. **Authentification** (entrer un nom et un canal)
3. **Interaction** - Affichage et saisie de messages
4. **Commandes** - /help, /quit, /switch

### Includes et définitions

```c
#include <stdio.h>           // Entrée/sortie standard (printf, fgets, etc.)
#include <stdlib.h>          // Utilitaires (malloc, exit, etc.)
#include <string.h>          // Manipulation de chaînes (strlen, strcat, etc.)
#include <unistd.h>          // POSIX API (close, sleep, etc.)
#include <arpa/inet.h>       // SOCKETS - Fonctions réseau (inet_addr, htons, etc.)
#include <time.h>            // Gestion du temps (time, localtime, strftime, etc.)
#include <sys/stat.h>        // Statistiques de fichiers (mkdir, etc.)
#include <ctype.h>           // Manipulation de caractères (tolower, etc.)

#define BUFFER_SIZE 1024     // Taille max d'un message
#define PORT 12345           // Port du serveur
#define ADRESSE_IP "127.0.0.1"  // IP du serveur (localhost)
```

### Fonction: to_lowercase()

```c
void to_lowercase(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = tolower(str[i]);
    }
}
```

**Objectif:** Convertir une chaîne en minuscules

**Paramètre:**

- `char *str` - Pointeur vers la chaîne à modifier (passée par référence)

**Explication:**

- Boucle caractère par caractère jusqu'à `\0` (fin de chaîne)
- `tolower()` convertit chaque caractère en minuscule

**Pourquoi?** Pour normaliser les noms de canaux (garder une cohérence)

### Fonction: format_current_time()

```c
void format_current_time(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);                           // Temps actuel
    struct tm *tm_info = localtime(&now);              // Convertir en structure
    strftime(buffer, buffer_size, "%d/%m/%Y %H:%M:%S", tm_info);
}
```

**Types explicités:**

- `time_t` - Type représentant le temps (secondes depuis 1970)
- `struct tm` - Structure contenant jour, mois, année, heure, minute, seconde
- `size_t` - Type non-signé pour les tailles (toujours ≥ 0)

**Flux d'exécution:**

1. `time(NULL)` → Obtient l'heure actuelle
2. `localtime()` → Convertit en structure exploitable
3. `strftime()` → Formate en chaîne "JJ/MM/AAAA HH:MM:SS"

**Exemple:**

```
Buffer rempli avec: "06/01/2026 14:30:45"
```

### Fonction: display_history_and_prompt()

```c
void display_history_and_prompt(const char *history) {
    system("clear");                                    // Effacer l'écran
    printf("%s", history);                              // Afficher l'historique
    printf("\n%s", "Envoyer un message : ");            // Afficher le prompt
    fflush(stdout);                                     // Forcer l'affichage
}
```

**Détails:**

- `system("clear")` - Exécute la commande `clear` du shell
- `fflush(stdout)` - Important! Force l'affichage immédiat (sans attendre)
  - Sans cela, le texte resterait en mémoire tampon

### Fonction: chat() - Le cœur du client

C'est la plus complexe, elle gère l'interaction avec le serveur.

```c
void chat(int client_socket, char *channel_name) {
    char buffer[BUFFER_SIZE];
    char history[BUFFER_SIZE * 10] = "";
    fd_set read_fds;
```

**Variables locales:**

- `buffer[1024]` - Stockage temporaire des messages reçus/envoyés
- `history[10240]` - Historique local affiché à l'écran (10 × 1024)
- `fd_set read_fds` - Structure pour surveiller les sockets

#### Comprendre `select()` et `fd_set`

**Le problème:**
Le client doit:

- Recevoir les messages du serveur (socket)
- Recevoir l'entrée utilisateur (clavier/stdin)

Comment gérer les deux en même temps?

**La solution: select()**

`select()` est une fonction qui **attend** que l'un des deux événements se produise:

```c
select(client_socket + 1, &read_fds, NULL, NULL, NULL);
```

**Paramètres:**

1. `client_socket + 1` - Nombre de descripteurs à surveiller
2. `&read_fds` - Ensemble de descripteurs à LIRE
3. `NULL` - Pas de descripteurs à ÉCRIRE
4. `NULL` - Pas de descripteurs d'erreur
5. `NULL` - Timeout (NULL = attendre indéfiniment)

**Flux:**

```
FD_ZERO(&read_fds);              // Vider l'ensemble
FD_SET(STDIN_FILENO, &read_fds); // Ajouter le clavier (file descriptor 0)
FD_SET(client_socket, &read_fds);// Ajouter le socket (file descriptor du serveur)

select(...);  // Attendre un événement

// Après select(), on vérifie quel événement s'est produit:

if (FD_ISSET(client_socket, &read_fds)) {
    // Message reçu du serveur
}

if (FD_ISSET(STDIN_FILENO, &read_fds)) {
    // Utilisateur a tapé quelque chose
}
```

#### Réception d'un message serveur

```c
if (FD_ISSET(client_socket, &read_fds)) {
    int read_size = recv(client_socket, buffer, sizeof(buffer), 0);
    if (read_size <= 0) {
        printf("Déconnecté du serveur\n");
        break;
    }
    buffer[read_size] = '\0';           // Ajouter le caractère nul
    strcat(history, buffer);             // Ajouter à l'historique
    display_history_and_prompt(history); // Réafficher
}
```

**Détails:**

- `recv()` bloque jusqu'à réception de données (ou déconnexion)
- `read_size` = nombre d'octets reçus
- `read_size <= 0` signifie déconnexion
- `buffer[read_size] = '\0'` termine la chaîne

#### Traitement de l'entrée utilisateur

```c
if (FD_ISSET(STDIN_FILENO, &read_fds)) {
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = '\0';  // Supprimer le '\n'
```

**Détail important:** `strcspn()`

- Cherche le premier '\n' dans la chaîne
- Le remplace par '\0' pour "nettoyer" l'entrée

**Exemple:**

```
Avant: "Bonjour\n\0"
Après: "Bonjour\0"
```

#### Gestion des commandes

```c
if (buffer[0] == '/') {
    to_lowercase(buffer);  // Normaliser en minuscules

    if (strcmp(buffer, "/quit") == 0) {
        // Quitter
    }
    else if (strncmp(buffer, "/switch ", 8) == 0) {
        // Changer de canal
    }
    else if (strcmp(buffer, "/help") == 0) {
        // Afficher l'aide
    }
    else {
        // Commande inconnue
    }
}
```

**Cas 1: /quit**

```c
system("clear");
printf("Déconnexion...\n");
break;  // Sortir de la boucle while
```

**Cas 2: /switch [nouveau_channel]**

```c
char new_channel[BUFFER_SIZE];
sscanf(buffer + 8, "%s", new_channel);  // Extraire le nom du canal

send(client_socket, buffer, strlen(buffer), 0);  // Envoyer au serveur
strncpy(channel_name, new_channel, sizeof(channel_name) - 1);
snprintf(history, sizeof(history), "Changement vers le channel '%s'\n", channel_name);
display_history_and_prompt(history);
continue;  // Sauter l'envoi normal du message
```

**Cas 3: /help**

```c
system("clear");
printf("\n\nCommandes disponibles :\n");
printf("-------------------------\n");
printf("/quit             : Quitter le chat\n");
printf("/switch [channel] : Changer de channel\n");
printf("/help             : Afficher cette aide\n\n");
printf("Appuyez sur Entrée pour revenir au chat...\n");
getchar();  // Attendre l'entrée utilisateur
display_history_and_prompt(history);
continue;
```

#### Envoi d'un message normal

```c
if (send(client_socket, buffer, strlen(buffer), 0) == -1) {
    perror("Erreur lors de l'envoi du message");
    break;
}

char time_buffer[26];
format_current_time(time_buffer, sizeof(time_buffer));

char formatted_message[BUFFER_SIZE];
snprintf(formatted_message, sizeof(formatted_message),
         "[%s] (%s) Moi : %s\n", channel_name, time_buffer, buffer);

strcat(history, formatted_message);
display_history_and_prompt(history);
```

**Flux:**

1. Envoyer le message brut au serveur
2. Formater le message avec timestamp
3. Ajouter à l'historique local
4. Réafficher

### Fonction main() du client

```c
int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char user_name[50];
    char channel_name[50];
```

**Types explicités:**

- `int client_socket` - Identifiant du socket (file descriptor)
- `struct sockaddr_in` - Structure réseau

#### Étapes:

**1. Créer le socket**

```c
client_socket = socket(AF_INET, SOCK_STREAM, 0);
if (client_socket == -1) {
    perror("Erreur lors de la création du socket");
    exit(EXIT_FAILURE);
}
```

- `AF_INET` - IPv4
- `SOCK_STREAM` - TCP (fiable, en ordre)
- `0` - Protocole par défaut

**2. Configurer l'adresse du serveur**

```c
server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = inet_addr(ADRESSE_IP);  // "127.0.0.1"
server_addr.sin_port = htons(PORT);                    // 12345
```

- `inet_addr()` - Convertit "127.0.0.1" en format binaire
- `htons()` - Convertit le port en format réseau (big-endian)

**3. Établir la connexion**

```c
if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    perror("Erreur lors de la connexion au serveur");
    exit(EXIT_FAILURE);
}
```

**4. Authentification**

```c
printf("Entrez votre nom : ");
fgets(user_name, sizeof(user_name), stdin);
user_name[strcspn(user_name, "\n")] = '\0';
send(client_socket, user_name, strlen(user_name), 0);

printf("Entrez le nom du channel : ");
fgets(channel_name, sizeof(channel_name), stdin);
to_lowercase(channel_name);
channel_name[strcspn(channel_name, "\n")] = '\0';
send(client_socket, channel_name, strlen(channel_name), 0);
```

**5. Lancer le chat**

```c
chat(client_socket, channel_name);
close(client_socket);
return 0;
```

---

## Analyse Détaillée du SERVEUR {#serveur}

### Vue d'ensemble du serveur

Le serveur:

1. **Écoute** les connexions (port 12345)
2. **Accepte** les clients
3. **Crée un thread** pour chaque client
4. **Gère** les canaux (création, suppression)
5. **Persiste** les messages dans des fichiers
6. **Utilise un mutex** pour synchroniser l'accès aux données partagées

### Includes et définitions

```c
#include <stdio.h>          // Entrée/sortie
#include <stdlib.h>         // Utilitaires
#include <string.h>         // Chaînes
#include <unistd.h>         // POSIX
#include <pthread.h>        // THREADS - pthread_create, pthread_mutex_t, etc.
#include <arpa/inet.h>      // Sockets réseau
#include <sys/stat.h>       // Statistiques fichiers (mkdir, etc.)
#include <errno.h>          // Codes d'erreur
#include <stdint.h>         // Types entiers (uint32_t, etc.)
#include <time.h>           // Temps
#include <dirent.h>         // Répertoires
#include <sys/types.h>      // Types système

#define PORT 12345          // Port d'écoute
#define BUFFER_SIZE 1024    // Taille des messages
#define MAX_CLIENTS 100     // Clients max TOTAUX
#define MAX_CHANNELS 100    // Canaux max
```

### Structure Channel

```c
typedef struct {
    char name[50];              // Nom: "général", "musique", etc.
    int clients[MAX_CLIENTS];   // Sockets des clients connectés
    int client_count;           // Nombre actuel
} Channel;
```

### Variables globales et mutex

```c
Channel channels[MAX_CHANNELS];                      // Tous les canaux
int channel_count = 0;                               // Nombre de canaux créés
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;   // Verrou global
```

**Pourquoi un mutex global?**
Parce que plusieurs threads peuvent:

- Lire/modifier `channels[]` simultanément
- Accéder à `channel_count` en même temps
- Créer un nouveau canal au même moment

Sans verrou → **Race condition** (résultat imprévisible)

### Fonction: count_total_clients()

```c
int count_total_clients() {
    int total = 0;
    for (int i = 0; i < channel_count; ++i) {
        total += channels[i].client_count;
    }
    return total;
}
```

**Simple:** Compte tous les clients dans tous les canaux

**Attention:** Cette fonction doit être appelée **dans un bloc protégé par mutex**:

```c
pthread_mutex_lock(&mutex);
int total = count_total_clients();
pthread_mutex_unlock(&mutex);
```

### Fonction: get_storage_file_path()

```c
void get_storage_file_path(const char *channel_name, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size,
             "storage_server/storage_%s/history_channel_file_%s.txt",
             channel_name, channel_name);
}
```

**Objectif:** Générer le chemin du fichier pour un canal

**Exemple:**

```
Canal: "musique"
Chemin généré: "storage_server/storage_musique/history_channel_file_musique.txt"
```

**snprintf():**

- Formate une chaîne (comme printf mais dans une variable)
- Limite à `buffer_size` octets (sécurité)

### Fonction: ensure_channel_directory_and_file()

```c
void ensure_channel_directory_and_file(const char *channel_name) {
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "storage_server/storage_%s", channel_name);

    // Crée storage_server s'il n'existe pas
    if (mkdir("storage_server", 0777) == -1 && errno != EEXIST) {
        perror("Erreur lors de la création du dossier 'storage_server'");
        return;
    }

    // Crée storage_[channel] s'il n'existe pas
    if (mkdir(dir_path, 0777) == -1 && errno != EEXIST) {
        perror("Erreur lors de la création du dossier du channel");
        return;
    }

    // Crée ou ouvre le fichier d'historique
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/history_channel_file_%s.txt", dir_path, channel_name);

    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        // Fichier n'existe pas, le créer
        file = fopen(file_path, "a");
        if (file == NULL) {
            perror("Erreur lors de la création du fichier de stockage");
            return;
        }
        fclose(file);
    }
}
```

**Étapes:**

1. Créer le répertoire `storage_server/`
2. Créer `storage_server/storage_[channel]/`
3. Créer ou ouvrir le fichier `history_channel_file_[channel].txt`

**Gestion d'erreur importante:**

```c
if (mkdir(...) == -1 && errno != EEXIST) {
    // Si le répertoire existe déjà (errno == EEXIST), c'est OK
    // Si c'est une autre erreur, afficher
}
```

**Modes de fichier:**

- `"r"` - Lecture (échoue si n'existe pas)
- `"a"` - Ajout (crée si n'existe pas)

### Fonction: log_and_broadcast_message()

```c
void log_and_broadcast_message(const char *channel_name, const char *sender,
                                const char *message, Channel *channel, int sender_socket) {
    char file_path[256];
    get_storage_file_path(channel_name, file_path, sizeof(file_path));

    FILE *file = fopen(file_path, "a");
    if (file == NULL) {
        perror("Erreur lors de l'ouverture du fichier de stockage");
        return;
    }

    // Obtenir l'heure actuelle
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buffer[26];
    strftime(time_buffer, sizeof(time_buffer), "%d/%m/%Y %H:%M:%S", tm_info);

    // Formater le message
    char formatted_message[BUFFER_SIZE];
    snprintf(formatted_message, sizeof(formatted_message),
             "[%s] (%s) %s : %s\n", channel_name, time_buffer, sender, message);

    // Écrire dans le fichier
    fprintf(file, "%s", formatted_message);
    fclose(file);

    // Envoyer à tous les clients du channel SAUF l'expéditeur
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < channel->client_count; ++i) {
        if (channel->clients[i] != sender_socket) {
            send(channel->clients[i], formatted_message, strlen(formatted_message), 0);
        }
    }
    pthread_mutex_unlock(&mutex);
}
```

**Deux actions:**

1. **Sauvegarde:** Écrire dans le fichier

```
[general] (06/01/2026 14:30:45) Lucas : Bonjour tous!
```

2. **Diffusion:** Envoyer à tous les autres clients du canal

```
Clients du canal "general": [15, 18, 22]
Expéditeur: 15
Envoyer à: 18, 22
```

**Important:** Le mutex protège la boucle d'envoi (lecture de `channel->client_count`)

### Fonction: write_welcome_message()

```c
void write_welcome_message(const char *channel_name) {
    char file_path[256];
    get_storage_file_path(channel_name, file_path, sizeof(file_path));

    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Erreur lors de l'ouverture du fichier de stockage");
        return;
    }

    // Vérifier si le fichier est vide
    fseek(file, 0, SEEK_END);           // Aller à la fin
    if (ftell(file) == 0) {             // Si position = 0, fichier est vide
        fclose(file);

        // Écrire le message de bienvenue
        file = fopen(file_path, "a");
        if (file == NULL) {
            perror("Erreur lors de l'écriture du message de bienvenue");
            return;
        }

        fprintf(file, "Bienvenue dans le channel '%s' !\n", channel_name);
    }
    fclose(file);
}
```

**Vérifier si un fichier est vide:**

1. `fseek(file, 0, SEEK_END)` - Aller à la fin du fichier
2. `ftell(file)` - Obtenir la position actuelle
3. Si position = 0 → fichier est vide

**Résultat:** Chaque canal a un message de bienvenue au démarrage

### Fonction: send_storage_to_client()

```c
void send_storage_to_client(int client_socket, const char *channel_name) {
    char file_path[256], line[BUFFER_SIZE];
    get_storage_file_path(channel_name, file_path, sizeof(file_path));

    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Erreur lors de l'ouverture du fichier de stockage");
        return;
    }

    // Lire et envoyer ligne par ligne
    while (fgets(line, sizeof(line), file)) {
        send(client_socket, line, strlen(line), 0);
    }
    fclose(file);
}
```

**Objectif:** Quand un client rejoint un canal, lui envoyer tout l'historique

**Flux:**

```
Fichier: "Bienvenue...\nLucas: Bonjour\nMartin: Salut\n"

Envoi 1: "Bienvenue...\n"
Envoi 2: "Lucas: Bonjour\n"
Envoi 3: "Martin: Salut\n"
```

### Fonction: find_or_create_channel() - Très importante!

```c
Channel *find_or_create_channel(const char *channel_name) {
    pthread_mutex_lock(&mutex);  // ← PROTÉGER L'ACCÈS

    // Chercher si le canal existe déjà
    for (int i = 0; i < channel_count; ++i) {
        if (strcmp(channels[i].name, channel_name) == 0) {
            pthread_mutex_unlock(&mutex);
            return &channels[i];
        }
    }

    // Vérifier qu'on peut créer un nouveau canal
    if (channel_count >= MAX_CHANNELS) {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    // Créer le nouveau canal
    strncpy(channels[channel_count].name, channel_name,
            sizeof(channels[channel_count].name) - 1);
    channels[channel_count].name[sizeof(channels[channel_count].name) - 1] = '\0';
    channels[channel_count].client_count = 0;

    ensure_channel_directory_and_file(channel_name);
    write_welcome_message(channel_name);

    channel_count++;

    pthread_mutex_unlock(&mutex);
    return &channels[channel_count - 1];
}
```

**Raison du mutex:**
Deux threads ne doivent pas créer le même canal simultanément!

**Scénario sans mutex:**

```
Thread 1: Cherche "musique"  → Pas trouvé
Thread 2: Cherche "musique"  → Pas trouvé
Thread 1: Crée canal "musique" → channel_count = 1
Thread 2: Crée canal "musique" AUSSI → channel_count = 2  ❌ ERREUR!
```

**Avec mutex:**

```
Thread 1: [Verrouille] Cherche → Crée → [Déverrouille]
Thread 2:                    [Attend le verrou]
Thread 2: [Verrouille] Cherche → Trouve! → [Déverrouille]
```

### Fonction: broadcast_message()

```c
void broadcast_message(Channel *channel, const char *message, int sender_socket) {
    pthread_mutex_lock(&mutex);

    for (int i = 0; i < channel->client_count; ++i) {
        if (channel->clients[i] != sender_socket) {
            send(channel->clients[i], message, strlen(message), 0);
        }
    }

    pthread_mutex_unlock(&mutex);
}
```

**Simple diffusion:** Envoyer un message à tous les clients SAUF l'expéditeur

**Exemples d'utilisation:**

- Notification "X a rejoint le canal"
- Notification "X a quitté le canal"

### Fonction: remove_client_from_channel()

```c
void remove_client_from_channel(Channel *channel, int client_socket) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < channel->client_count; ++i) {
        if (channel->clients[i] == client_socket) {
            // Décaler tous les éléments après cette position
            for (int j = i; j < channel->client_count - 1; ++j) {
                channel->clients[j] = channel->clients[j + 1];
            }
            channel->client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}
```

**Objectif:** Retirer un client du tableau `clients[]`

**Exemple:**

```
Avant: clients = [15, 18, 22], count = 3
Retirer: 18
Après: clients = [15, 22, X], count = 2
```

### Fonction: handle_client() - Le cœur du serveur

```c
void *handle_client(void *args) {
    int client_socket = *((int *)args);
    free(args);

    char buffer[BUFFER_SIZE];
    char client_name[50];
    char channel_name[50];

    recv(client_socket, client_name, sizeof(client_name), 0);
    recv(client_socket, channel_name, sizeof(channel_name), 0);
```

**Type de retour:** `void *` (pointeur générique)

- Requis par `pthread_create()`
- On retourne `NULL`

**Premier paramètre:** `void *args`

- Pointeur opaque passé par `pthread_create()`
- On le caste en `int *` pour extraire le socket

**Libération mémoire:** `free(args)`

- La mémoire a été allouée dans `main()` avec `malloc()`
- On la libère ici (oui, c'est safe de libérer dans le thread)

#### Étape 1: Vérifier la limite de clients

```c
pthread_mutex_lock(&mutex);
int total_clients = count_total_clients();
pthread_mutex_unlock(&mutex);

if (total_clients >= MAX_CLIENTS) {
    send(client_socket, "Erreur : Le serveur est plein. Connexion refusée.\n", 51, 0);
    close(client_socket);
    return NULL;
}
```

#### Étape 2: Rejoindre/créer un canal

```c
Channel *channel = find_or_create_channel(channel_name);

if (channel == NULL) {
    close(client_socket);
    return NULL;
}

pthread_mutex_lock(&mutex);
channel->clients[channel->client_count++] = client_socket;
int current_count = channel->client_count;
pthread_mutex_unlock(&mutex);
```

**Important:** Sauvegarder `current_count` DANS le mutex

- Avant de déverrouiller, on a une "photo" du nombre de clients
- Si on le récupère après, il pourrait avoir changé

#### Étape 3: Envoyer l'historique

```c
send_storage_to_client(client_socket, channel_name);

// Notifier les autres
char join_message[BUFFER_SIZE];
snprintf(join_message, sizeof(join_message),
         "%s a rejoint le channel '%s'... (%d/%d)\n",
         client_name, channel_name, current_count, MAX_CLIENTS);
broadcast_message(channel, join_message, client_socket);
```

#### Étape 4: Boucle principale - Traitement des messages

```c
while (1) {
    int read_size = recv(client_socket, buffer, sizeof(buffer), 0);
    if (read_size <= 0) {
        break;  // Déconnecté
    }

    buffer[read_size] = '\0';

    // Cas 1: Changement de canal
    if (strncmp(buffer, "/switch ", 8) == 0) {
        // ... traitement complexe ...
        continue;
    }

    // Cas 2: Message normal
    log_and_broadcast_message(channel_name, client_name, buffer, channel, client_socket);
}
```

#### Traitement du /switch

```c
if (strncmp(buffer, "/switch ", 8) == 0) {
    char new_channel_name[50];
    sscanf(buffer + 8, "%s", new_channel_name);  // Extraire le nom

    Channel *new_channel = find_or_create_channel(new_channel_name);

    if (new_channel == NULL) {
        send(client_socket, "Erreur : Impossible de rejoindre le nouveau channel\n", 55, 0);
        close(client_socket);
        return NULL;
    }

    // Notifier l'ancien canal
    pthread_mutex_lock(&mutex);
    int remaining_clients = channel->client_count - 1;
    pthread_mutex_unlock(&mutex);

    char leave_message[BUFFER_SIZE];
    snprintf(leave_message, sizeof(leave_message),
             "%s a quitter le channel '%s'... (%d/%d)\n",
             client_name, channel_name, remaining_clients, MAX_CLIENTS);
    broadcast_message(channel, leave_message, client_socket);

    remove_client_from_channel(channel, client_socket);

    // Ajouter au nouveau canal
    pthread_mutex_lock(&mutex);
    new_channel->clients[new_channel->client_count++] = client_socket;
    int new_count = new_channel->client_count;
    pthread_mutex_unlock(&mutex);

    send_storage_to_client(client_socket, new_channel_name);

    // Notifier le nouveau canal
    char join_message[BUFFER_SIZE];
    snprintf(join_message, sizeof(join_message),
             "%s a rejoint le channel '%s'... (%d/%d)\n",
             client_name, new_channel_name, new_count, MAX_CLIENTS);
    broadcast_message(new_channel, join_message, client_socket);

    // Mettre à jour les variables locales du thread
    channel = new_channel;
    strncpy(channel_name, new_channel_name, sizeof(channel_name) - 1);
    channel_name[sizeof(channel_name) - 1] = '\0';

    // Confirmer au client
    char switch_message[BUFFER_SIZE];
    snprintf(switch_message, sizeof(switch_message),
             "Vous avez rejoint le channel '%s'\n", new_channel_name);
    send(client_socket, switch_message, strlen(switch_message), 0);

    continue;
}
```

**Flux complet du /switch:**

1. Extraire le nouveau nom du canal
2. Trouver ou créer le canal
3. Notifier l'ancien canal de la départ
4. Retirer du canal actuel
5. Ajouter au nouveau canal
6. Envoyer l'historique du nouveau
7. Notifier le nouveau canal de l'arrivée
8. Mettre à jour les variables locales du thread

#### Étape 5: Déconnexion

```c
// Notifier les autres
pthread_mutex_lock(&mutex);
int remaining_clients = channel->client_count - 1;
pthread_mutex_unlock(&mutex);

char leave_message[BUFFER_SIZE];
snprintf(leave_message, sizeof(leave_message),
         "%s a quitter le channel '%s'... (%d/%d)\n",
         client_name, channel_name, remaining_clients, MAX_CLIENTS);

broadcast_message(channel, leave_message, client_socket);
remove_client_from_channel(channel, client_socket);
close(client_socket);
return NULL;  // Fin du thread
```

### Fonction main() du serveur

```c
int main() {
    int server_socket, client_socket, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
```

**Types:**

- `int server_socket` - Socket d'écoute
- `int client_socket` - Socket client accepté
- `int *new_sock` - Pointeur pour passer le socket au thread
- `struct sockaddr_in` - Adresses IP/port
- `socklen_t` - Type pour les longueurs

#### Étape 1: Créer le socket serveur

```c
server_socket = socket(AF_INET, SOCK_STREAM, 0);
if (server_socket == -1) {
    perror("Erreur lors de la création du socket");
    exit(EXIT_FAILURE);
}
```

#### Étape 2: Configurer et attacher

```c
server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = INADDR_ANY;  // Écouter sur toutes les interfaces
server_addr.sin_port = htons(PORT);        // Port 12345

if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Erreur lors du bind");
    close(server_socket);
    exit(EXIT_FAILURE);
}
```

**INADDR_ANY:** Écouter sur toutes les interfaces réseau (0.0.0.0)

#### Étape 3: Écouter les connexions

```c
if (listen(server_socket, 3) < 0) {
    perror("Erreur lors de l'écoute");
    close(server_socket);
    exit(EXIT_FAILURE);
}

printf("Serveur en écoute sur le port %d...\n", PORT);
```

**Paramètre `3`:** Backlog (queue de connexions en attente)

#### Étape 4: Boucle d'acceptation

```c
while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size))) {
    pthread_t thread_id;
    new_sock = malloc(sizeof(int));  // Allouer mémoire
    *new_sock = client_socket;       // Copier le socket

    if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0) {
        perror("Erreur lors de la création du thread");
        free(new_sock);
        close(client_socket);
    }

    pthread_detach(thread_id);  // Libérer les ressources automatiquement
}
```

**Pourquoi malloc?**

- `pthread_create()` exécute la fonction de façon **asynchrone**
- Si on passait `&client_socket` (adresse stack), la valeur changerait
- On crée une copie heap qui persiste jusqu'à ce que le thread la lise

**pthread_detach:**

- Normalement, il faut faire `pthread_join()` pour attendre la fin d'un thread
- `pthread_detach()` dit: "Libère les ressources automatiquement quand il finit"
- Sans cela, les threads "morts" resteraient en mémoire

#### Étape 5: Cleanup

```c
if (client_socket < 0) {
    perror("Erreur lors de l'acceptation");
}

close(server_socket);
return 0;
```

---

## Flux de Communication {#flux-de-communication}

### Scénario: Deux clients discutent

```
ÉTAPE 1: CLIENT A ENVOIE UN MESSAGE
═════════════════════════════════════

CLIENT A                                SERVER                              CLIENT B
─────────────────────────────────────────────────────────────────────────────────────
Tape "Bonjour"
    │
    ├─→ send() ─────────────────────→ recv() (Thread A)
                                        │
                                        ├─→ Formater le message
                                        │   "[général] (14:30:45) Alice : Bonjour"
                                        │
                                        ├─→ Écrire dans le fichier
                                        │
                                        ├─→ broadcast_message()
                                        │   (Envoyer à tous SAUF Alice)
                                        │
                                        └─→ send() ──────────────────→ recv()
                                                                          │
                                                                          ├─→ strcat(history)
                                                                          │
                                                                          ├─→ display()
                                                                          │
                                                                          Voit le message!

ÉTAPE 2: CLIENT B RÉPOND
═════════════════════════════════════

CLIENT B                                SERVER                              CLIENT A
─────────────────────────────────────────────────────────────────────────────────────
Tape "Bonjour Alice!"
    │
    ├─→ send() ─────────────────────→ recv() (Thread B)
                                        │
                                        ├─→ Formater le message
                                        │   "[général] (14:30:46) Bob : Bonjour Alice!"
                                        │
                                        ├─→ Écrire dans le fichier
                                        │
                                        ├─→ broadcast_message()
                                        │
                                        └─→ send() ──────────────────→ recv()
                                                                          │
                                                                          Voit la réponse!
```

### Scénario: Changement de canal

```
CLIENT A (Alice)                        SERVER                          CLIENT B (Bob)
   (général)                                                              (général)
─────────────────────────────────────────────────────────────────────────────────────

Tape "/switch musique"
    │
    ├─→ send("/switch musique") ────→ Thread A reçoit
                                        │
                                        ├─→ find_or_create_channel("musique")
                                        │
                                        ├─→ Notifier général:
                                        │   "Alice a quitté..."
                                        │   ────────────────────────→ Bob voit
                                        │
                                        ├─→ Envoyer historique musique
                                        │   ←─── Alice reçoit
                                        │
                                        ├─→ Notifier musique:
                                        │   "Alice a rejoint..."

Maintenant:
CLIENT A est dans "musique"
CLIENT B est dans "général"
(Ils ne voient plus leurs messages mutuels!)
```

---

## Gestion de la Concurrence {#gestion-de-la-concurrence}

### Race Conditions (Conditions de course)

**Définition:** Deux threads accèdent à la même ressource au même moment, résultant en un état incohérent.

#### Exemple 1: Sans mutex

```c
// ❌ DANGEREUX - Sans protection
Channel channels[MAX_CHANNELS];
int channel_count = 0;

// Thread 1                          Thread 2
┌────────────────────────┐          ┌────────────────────────┐
│ Cherche "musique"      │          │ Cherche "musique"      │
│ count = 2              │          │ count = 2              │
│ Pas trouvé             │          │ Pas trouvé             │
│ Crée "musique"         │          │ Crée "musique" AUSSI!  │
│ count = 3              │          │ count = 3              │
│ channels[2] = musique1 │          │ channels[2] = musique2 │
└────────────────────────┘          └────────────────────────┘

Résultat: "musique" en double! ❌
```

#### Exemple 2: Avec mutex

```c
// ✅ SAFE - Avec protection
pthread_mutex_lock(&mutex);   // ← VERROU

// Thread 1                          Thread 2
┌────────────────────────┐
│ Cherche "musique"      │          [ATTEND LE VERROU]
│ count = 2              │
│ Pas trouvé             │
│ Crée "musique"         │
│ count = 3              │
│ channels[2] = musique1 │
└────────────────────────┘
                                    ┌────────────────────────┐
pthread_mutex_unlock(&mutex);       │ Cherche "musique"      │
                                    │ count = 3              │
                                    │ TROUVE! (channels[2])  │
                                    │ Retour channels[2]     │
                                    └────────────────────────┘

Résultat: Canal "musique" utilisé correctement! ✅
```

### Sections critiques dans notre code

**Sections critiques** = zones qui accèdent à des données partagées

| Fonction                       | Données partagées                             | Pourquoi mutex?                                 |
| ------------------------------ | --------------------------------------------- | ----------------------------------------------- |
| `find_or_create_channel()`     | `channels[]`, `channel_count`                 | Deux threads ne doivent pas créer le même canal |
| `broadcast_message()`          | `channel->clients[]`, `channel->client_count` | La liste des clients peut changer               |
| `remove_client_from_channel()` | `channel->clients[]`, `channel->client_count` | Modification du tableau                         |
| `log_and_broadcast_message()`  | `channel->clients[]`                          | Lecture du nombre de clients                    |
| `handle_client()`              | `channels[]`, `channel_count`                 | Accès aux canaux                                |

### Bonnes pratiques appliquées

✅ **Verrous courts:** Libérer le mutex dès que possible

```c
pthread_mutex_lock(&mutex);
int count = channel->client_count;  // Copier la valeur
pthread_mutex_unlock(&mutex);

// Utiliser 'count' sans verrou
printf("%d clients\n", count);
```

✅ **Cohérence:** Tout accès à `channels[]` est protégé

✅ **Pas de deadlock:** On utilise un SEUL mutex (pas plusieurs en même temps)

---

## Exécution et Compilation {#exécution-et-compilation}

### Compilation

```bash
# Compiler le serveur
gcc -o server server.c -lpthread

# Compiler le client
gcc -o client client.c

# Pourquoi -lpthread?
# C'est une bibliothèque pour les threads
# Elle contient pthread_create, pthread_mutex_lock, etc.
```

### Exécution

**Terminal 1 (Serveur):**

```bash
$ ./server
Serveur en écoute sur le port 12345...
```

**Terminal 2 (Client 1):**

```bash
$ ./client
Entrez votre nom : Alice
Entrez le nom du channel : général
[à afficher l'historique du canal]
Envoyer un message :
```

**Terminal 3 (Client 2):**

```bash
$ ./client
Entrez votre nom : Bob
Entrez le nom du channel : général
[à afficher l'historique du canal]
Envoyer un message :
```

### Structure de fichiers créée

```
MonProjet/
├── client
├── server
├── client.c
├── server.c
├── HELP.md
├── README.md
└── storage_server/
    ├── storage_général/
    │   └── history_channel_file_général.txt
    │       "Bienvenue dans le channel 'général' !"
    │       "[général] (06/01/2026 14:30:45) Alice : Bonjour"
    │       "[général] (06/01/2026 14:30:46) Bob : Bonjour Alice!"
    │
    └── storage_musique/
        └── history_channel_file_musique.txt
            "Bienvenue dans le channel 'musique' !"
            "[musique] (06/01/2026 14:31:00) Charlie : Vous aimez qui comme artiste?"
```

---

## Résumé pour la présentation

### Points clés à retenir

1. **Sockets:** Communication réseau client-serveur via TCP
2. **Threads:** Un thread par client permet le traitement parallèle
3. **Mutex:** Protège l'accès à `channels[]` et `channel_count`
4. **Canaux:** Structure contenant le nom et la liste des clients
5. **Persistance:** Chaque message est sauvegardé dans un fichier

### Points techniques importants

- **Gestion de la mémoire:** `malloc()` dans main, `free()` dans le thread
- **Formats de message:** `[canal] (heure) utilisateur : message`
- **Synchronisation:** Mutex protège les sections critiques
- **Broadcast:** Envoyer à tous les clients SAUF l'expéditeur
- **Changement de canal:** Départ + Arrivée = 2 notifications

### Ce qui fait la robustesse du projet

✅ Gestion des erreurs (malloc, socket, fork)  
✅ Protection contre les race conditions  
✅ Nettoyage propre (close, free, pthread_detach)  
✅ Limitation du nombre de clients (MAX_CLIENTS)  
✅ Limitation du nombre de canaux (MAX_CHANNELS)  
✅ Historique persistant dans les fichiers

---

**Fin de la documentation technique.**

Pour toute question ou clarification supplémentaire, n'hésitez pas à consulter les commentaires dans le code source.

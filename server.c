#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>

#define PORT 12345
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
#define MAX_CHANNELS 100

typedef struct
{
    char name[50];
    int clients[MAX_CLIENTS];
    int client_count;
} Channel;

Channel channels[MAX_CHANNELS];
int channel_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Compte le nombre total de clients connectés au serveur.
 * @return Le nombre total de clients.
 */
int count_total_clients()
{
    int total = 0;
    for (int i = 0; i < channel_count; ++i)
    {
        total += channels[i].client_count;
    }
    return total;
}

/**
 * Obtient le chemin du fichier de stockage pour un channel.
 * @param channel_name Le nom du channel.
 * @param buffer Le buffer où le chemin sera stocké.
 * @param buffer_size La taille du buffer.
 */
void get_storage_file_path(const char *channel_name, char *buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size, "storage_server/storage_%s/history_channel_file_%s.txt", channel_name, channel_name);
}

/**
 * Assure la création du répertoire et du fichier de stockage pour un channel.
 * @param channel_name Le nom du channel.
 */
void ensure_channel_directory_and_file(const char *channel_name)
{
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "storage_server/storage_%s", channel_name);

    // Crée le répertoire pour le channel s'il n'existe pas
    if (mkdir("storage_server", 0777) == -1 && errno != EEXIST)
    {
        perror("Erreur lors de la création du dossier 'storage_server'");
        return;
    }

    if (mkdir(dir_path, 0777) == -1 && errno != EEXIST)
    {
        perror("Erreur lors de la création du dossier du channel");
        return;
    }

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/history_channel_file_%s.txt", dir_path, channel_name);

    FILE *file = fopen(file_path, "r");
    if (file == NULL)
    {
        // Si le fichier n'existe pas, le créer
        file = fopen(file_path, "a");
        if (file == NULL)
        {
            perror("Erreur lors de la création du fichier de stockage");
            return;
        }
        fclose(file);
    }
}

/**
 * Log un message dans le fichier de stockage d'un channel et l'envoie à tous les clients.
 * @param channel_name Le nom du channel.
 * @param sender Le nom de l'expéditeur.
 * @param message Le message à logger.
 * @param channel Le channel auquel envoyer le message.
 * @param sender_socket Le socket de l'expéditeur.
 */
void log_and_broadcast_message(const char *channel_name, const char *sender, const char *message, Channel *channel, int sender_socket)
{
    char file_path[256];
    get_storage_file_path(channel_name, file_path, sizeof(file_path));

    FILE *file = fopen(file_path, "a");
    if (file == NULL)
    {
        perror("Erreur lors de l'ouverture du fichier de stockage");
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buffer[26];
    strftime(time_buffer, sizeof(time_buffer), "%d/%m/%Y %H:%M:%S", tm_info);

    char formatted_message[BUFFER_SIZE];
    snprintf(formatted_message, sizeof(formatted_message), "[%s] (%s) %s : %s\n", channel_name, time_buffer, sender, message);

    fprintf(file, "%s", formatted_message);
    fclose(file);

    // Envoyer le message formaté à tous les clients du channel sauf l'expéditeur
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < channel->client_count; ++i)
    {
        if (channel->clients[i] != sender_socket)
        {
            send(channel->clients[i], formatted_message, strlen(formatted_message), 0);
        }
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * Écrit le message de bienvenue dans le fichier de stockage d'un channel.
 * @param channel_name Le nom du channel.
 */
void write_welcome_message(const char *channel_name)
{
    char file_path[256];
    get_storage_file_path(channel_name, file_path, sizeof(file_path));

    FILE *file = fopen(file_path, "r");
    if (file == NULL)
    {
        perror("Erreur lors de l'ouverture du fichier de stockage");
        return;
    }

    // Vérifie si le fichier est vide
    fseek(file, 0, SEEK_END);
    if (ftell(file) == 0)
    {
        fclose(file);

        // Écrit le message de bienvenue si le fichier est vide
        file = fopen(file_path, "a");
        if (file == NULL)
        {
            perror("Erreur lors de l'écriture du message de bienvenue");
            return;
        }

        fprintf(file, "Bienvenue dans le channel '%s' !\n", channel_name);
    }
    fclose(file);
}

/**
 * Envoie le contenu du fichier de stockage d'un channel à un client.
 * @param client_socket Le socket du client.
 * @param channel_name Le nom du channel.
 */
void send_storage_to_client(int client_socket, const char *channel_name)
{
    char file_path[256], line[BUFFER_SIZE];
    get_storage_file_path(channel_name, file_path, sizeof(file_path));

    FILE *file = fopen(file_path, "r");
    if (file == NULL)
    {
        perror("Erreur lors de l'ouverture du fichier de stockage");
        return;
    }

    while (fgets(line, sizeof(line), file))
    {
        send(client_socket, line, strlen(line), 0);
    }
    fclose(file);
}

/**
 * Trouve ou crée un channel.
 * @param channel_name Le nom du channel.
 * @return Le pointeur vers le channel trouvé ou créé.
 */
Channel *find_or_create_channel(const char *channel_name)
{
    pthread_mutex_lock(&mutex);

    for (int i = 0; i < channel_count; ++i)
    {
        if (strcmp(channels[i].name, channel_name) == 0)
        {
            pthread_mutex_unlock(&mutex);
            return &channels[i];
        }
    }

    if (channel_count >= MAX_CHANNELS)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    strncpy(channels[channel_count].name, channel_name, sizeof(channels[channel_count].name) - 1);
    channels[channel_count].name[sizeof(channels[channel_count].name) - 1] = '\0';
    channels[channel_count].client_count = 0;
    ensure_channel_directory_and_file(channel_name); // Crée le dossier et le fichier du channel
    write_welcome_message(channel_name);             // Écrit le message de bienvenue si nécessaire
    channel_count++;

    pthread_mutex_unlock(&mutex);
    return &channels[channel_count - 1];
}

/**
 * Diffuse un message à tous les clients d'un channel.
 * @param channel Le channel.
 * @param message Le message à diffuser.
 * @param sender_socket Le socket de l'expéditeur.
 */
void broadcast_message(Channel *channel, const char *message, int sender_socket)
{
    pthread_mutex_lock(&mutex);

    for (int i = 0; i < channel->client_count; ++i)
    {
        if (channel->clients[i] != sender_socket)
        {
            send(channel->clients[i], message, strlen(message), 0);
        }
    }

    pthread_mutex_unlock(&mutex);
}

/**
 * Supprime un client d'un channel.
 * @param channel Le channel.
 * @param client_socket Le socket du client à supprimer.
 */
void remove_client_from_channel(Channel *channel, int client_socket)
{
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < channel->client_count; ++i)
    {
        if (channel->clients[i] == client_socket)
        {
            for (int j = i; j < channel->client_count - 1; ++j)
            {
                channel->clients[j] = channel->clients[j + 1];
            }
            channel->client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * Gère les connexions des clients.
 * @param args Les arguments passés à la fonction.
 * @return NULL.
 */
void *handle_client(void *args)
{
    int client_socket = *((int *)args);
    free(args);

    char buffer[BUFFER_SIZE];
    char client_name[50];
    char channel_name[50];

    recv(client_socket, client_name, sizeof(client_name), 0);
    recv(client_socket, channel_name, sizeof(channel_name), 0);

    // Vérifier si le nombre maximum de clients est dépassé
    pthread_mutex_lock(&mutex);
    int total_clients = count_total_clients();
    pthread_mutex_unlock(&mutex);

    if (total_clients >= MAX_CLIENTS)
    {
        send(client_socket, "Erreur : Le serveur est plein. Connexion refusée.\n", 51, 0);
        close(client_socket);
        return NULL;
    }

    Channel *channel = find_or_create_channel(channel_name);

    if (channel == NULL)
    {
        close(client_socket);
        return NULL;
    }

    pthread_mutex_lock(&mutex);
    channel->clients[channel->client_count++] = client_socket;
    int current_count = channel->client_count;
    pthread_mutex_unlock(&mutex);

    send_storage_to_client(client_socket, channel_name);

    // Envoyer un message d'entrée à tous les clients du channel (sauf le nouveau)
    char join_message[BUFFER_SIZE];
    snprintf(join_message, sizeof(join_message), "%s a rejoint le channel '%s'... (%d/%d)\n", client_name, channel_name, current_count, MAX_CLIENTS);
    broadcast_message(channel, join_message, client_socket);

    while (1)
    {
        int read_size = recv(client_socket, buffer, sizeof(buffer), 0);
        if (read_size <= 0)
        {
            break;
        }

        buffer[read_size] = '\0';

        if (strncmp(buffer, "/switch ", 8) == 0)
        {
            char new_channel_name[50];
            sscanf(buffer + 8, "%s", new_channel_name);

            Channel *new_channel = find_or_create_channel(new_channel_name);

            if (new_channel == NULL)
            {
                send(client_socket, "Erreur : Impossible de rejoindre le nouveau channel\n", 55, 0);
                close(client_socket);
                return NULL;
            }

            // Envoyer un message de départ à l'ancien channel
            pthread_mutex_lock(&mutex);
            int remaining_clients = channel->client_count - 1;
            pthread_mutex_unlock(&mutex);

            char leave_message[BUFFER_SIZE];
            snprintf(leave_message, sizeof(leave_message), "%s a quitter le channel '%s'... (%d/%d)\n", client_name, channel_name, remaining_clients, MAX_CLIENTS);
            broadcast_message(channel, leave_message, client_socket);

            remove_client_from_channel(channel, client_socket);

            pthread_mutex_lock(&mutex);
            new_channel->clients[new_channel->client_count++] = client_socket;
            int new_count = new_channel->client_count;
            pthread_mutex_unlock(&mutex);

            send_storage_to_client(client_socket, new_channel_name);

            // Envoyer un message d'entrée au nouveau channel
            char join_message[BUFFER_SIZE];
            snprintf(join_message, sizeof(join_message), "%s a rejoint le channel '%s'... (%d/%d)\n", client_name, new_channel_name, new_count, MAX_CLIENTS);
            broadcast_message(new_channel, join_message, client_socket);

            channel = new_channel;
            strncpy(channel_name, new_channel_name, sizeof(channel_name) - 1);
            channel_name[sizeof(channel_name) - 1] = '\0';

            char switch_message[BUFFER_SIZE];
            snprintf(switch_message, sizeof(switch_message), "Vous avez rejoint le channel '%s'\n", new_channel_name);
            send(client_socket, switch_message, strlen(switch_message), 0);

            continue;
        }

        char formatted_message[BUFFER_SIZE];
        snprintf(formatted_message, sizeof(formatted_message), "%s : %s\n", client_name, buffer);

        log_and_broadcast_message(channel_name, client_name, buffer, channel, client_socket);
    }

    // Envoyer un message de départ à tous les clients du channel (sauf celui qui part)
    pthread_mutex_lock(&mutex);
    int remaining_clients = channel->client_count - 1;
    pthread_mutex_unlock(&mutex);

    char leave_message[BUFFER_SIZE];
    snprintf(leave_message, sizeof(leave_message), "%s a quitter le channel '%s'... (%d/%d)\n", client_name, channel_name, remaining_clients, MAX_CLIENTS);

    broadcast_message(channel, leave_message, client_socket);
    remove_client_from_channel(channel, client_socket);
    close(client_socket);
    return NULL;
}

int main()
{
    int server_socket, client_socket, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Erreur lors du bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 3) < 0)
    {
        perror("Erreur lors de l'écoute");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Serveur en écoute sur le port %d...\n", PORT);

    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size)))
    {
        pthread_t thread_id;
        new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0)
        {
            perror("Erreur lors de la création du thread");
            free(new_sock);
            close(client_socket);
        }

        pthread_detach(thread_id);
    }

    if (client_socket < 0)
    {
        perror("Erreur lors de l'acceptation");
    }

    close(server_socket);
    return 0;
}

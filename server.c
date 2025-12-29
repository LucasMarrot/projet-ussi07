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
    int port;
    int clients[MAX_CLIENTS];
    int client_count;
    int is_new; // Indique si le channel est nouveau
} Channel;

Channel channels[MAX_CHANNELS];
int channel_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Génère un port pour un channel en utilisant un hash du nom du channel.
 * @param channel_name Le nom du channel.
 * @return Le port généré.
 */
int generate_port_for_channel(const char *channel_name)
{
    uint32_t hash = 0;
    for (size_t i = 0; channel_name[i] != '\0'; i++)
    {
        hash = 31 * hash + channel_name[i];
    }
    return 1024 + (hash % 64512);
}

/**
 * Obtient le chemin du fichier de stockage pour un channel.
 * @param port Le port du channel.
 * @param buffer Le buffer où le chemin sera stocké.
 * @param buffer_size La taille du buffer.
 */
void get_storage_file_path(int port, char *buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size, "storage_server/storage_%d/history_channel_file_%d.txt", port, port);
}

/**
 * Assure la création du répertoire et du fichier de stockage pour un channel.
 * @param port Le port du channel.
 */
void ensure_channel_directory_and_file(int port)
{
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "storage_server/storage_%d", port);

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
    snprintf(file_path, sizeof(file_path), "%s/history_channel_file_%d.txt", dir_path, port);

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
 * Log un message dans le fichier de stockage d'un channel.
 * @param port Le port du channel.
 * @param sender Le nom de l'expéditeur.
 * @param message Le message à logger.
 * @param channel_name Le nom du channel.
 */
void log_message(int port, const char *sender, const char *message, const char *channel_name)
{
    char file_path[256];
    get_storage_file_path(port, file_path, sizeof(file_path));

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

    fprintf(file, "[%s] (%s) %s : %s\n", channel_name, time_buffer, sender, message);
    fclose(file);
}

/**
 * Écrit le message de bienvenue dans le fichier de stockage d'un channel.
 * @param port Le port du channel.
 * @param channel_name Le nom du channel.
 */
void write_welcome_message(int port, const char *channel_name)
{
    char file_path[256];
    get_storage_file_path(port, file_path, sizeof(file_path));

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
 * @param port Le port du channel.
 */
void send_storage_to_client(int client_socket, int port)
{
    char file_path[256], line[BUFFER_SIZE];
    get_storage_file_path(port, file_path, sizeof(file_path));

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
 * @param port Le port du channel.
 * @param channel_name Le nom du channel.
 * @return Le pointeur vers le channel trouvé ou créé.
 */
Channel *find_or_create_channel(int port, const char *channel_name)
{
    pthread_mutex_lock(&mutex);

    for (int i = 0; i < channel_count; ++i)
    {
        if (channels[i].port == port)
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

    channels[channel_count].port = port;
    channels[channel_count].client_count = 0;
    channels[channel_count].is_new = 1;
    ensure_channel_directory_and_file(port);   // Crée le dossier et le fichier du channel
    write_welcome_message(port, channel_name); // Écrit le message de bienvenue si nécessaire
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
 * Envoie un fichier à un client.
 * @param client_socket Le socket du client.
 * @param file_path Le chemin du fichier à envoyer.
 */
void send_file(int client_socket, const char *file_path)
{
    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
    {
        perror("Erreur lors de l'ouverture du fichier");
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t read_size;
    while ((read_size = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        send(client_socket, buffer, read_size, 0);
    }
    fclose(file);
}

/**
 * Liste les fichiers disponibles dans un channel.
 * @param client_socket Le socket du client.
 * @param port Le port du channel.
 */
void list_files(int client_socket, int port)
{
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "storage_server/storage_%d", port);

    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        perror("Erreur lors de l'ouverture du dossier");
        return;
    }

    struct dirent *entry;
    char message[BUFFER_SIZE] = "Fichiers disponibles :\n";
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG && strncmp(entry->d_name, "history_channel_file_", 21) != 0)
        {
            snprintf(message + strlen(message), sizeof(message) - strlen(message), "%s\n", entry->d_name);
        }
    }
    closedir(dir);

    send(client_socket, message, strlen(message), 0);
    send(client_socket, "Appuyez sur Entrée pour revenir au chat...\n", 40, 0);
    char buffer[BUFFER_SIZE];
    recv(client_socket, buffer, sizeof(buffer), 0); // Attendre que l'utilisateur appuie sur Entrée
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
    int port;

    recv(client_socket, client_name, sizeof(client_name), 0);
    recv(client_socket, channel_name, sizeof(channel_name), 0);

    port = generate_port_for_channel(channel_name);
    Channel *channel = find_or_create_channel(port, channel_name);

    if (channel == NULL)
    {
        close(client_socket);
        return NULL;
    }

    pthread_mutex_lock(&mutex);
    channel->clients[channel->client_count++] = client_socket;
    pthread_mutex_unlock(&mutex);

    send_storage_to_client(client_socket, port);

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

            int new_port = generate_port_for_channel(new_channel_name);
            Channel *new_channel = find_or_create_channel(new_port, new_channel_name);

            if (new_channel == NULL)
            {
                send(client_socket, "Erreur : Impossible de rejoindre le nouveau channel\n", 50, 0);
                continue;
            }

            remove_client_from_channel(channel, client_socket);

            pthread_mutex_lock(&mutex);
            new_channel->clients[new_channel->client_count++] = client_socket;
            pthread_mutex_unlock(&mutex);

            send_storage_to_client(client_socket, new_port);

            channel = new_channel;
            port = new_port;
            strncpy(channel_name, new_channel_name, sizeof(channel_name) - 1);
            channel_name[sizeof(channel_name) - 1] = '\0';

            char switch_message[BUFFER_SIZE];
            snprintf(switch_message, sizeof(switch_message), "Vous avez rejoint le channel '%s'\n", new_channel_name);
            send(client_socket, switch_message, strlen(switch_message), 0);

            continue;
        }

        if (strncmp(buffer, "/send ", 6) == 0)
        {
            char file_path[BUFFER_SIZE];
            sscanf(buffer + 6, "%s", file_path);

            FILE *file = fopen(file_path, "rb");
            if (file == NULL)
            {
                send(client_socket, "Erreur envoie du fichier\n", 25, 0);
                continue;
            }
            fclose(file);

            char storage_path[BUFFER_SIZE];
            snprintf(storage_path, sizeof(storage_path), "storage_server/storage_%d/%s", port, file_path);

            FILE *storage_file = fopen(storage_path, "wb");
            if (storage_file == NULL)
            {
                send(client_socket, "Erreur : Impossible de créer le fichier dans le stockage\n", 50, 0);
                continue;
            }

            file = fopen(file_path, "rb");
            char file_buffer[BUFFER_SIZE];
            size_t read_size;
            while ((read_size = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0)
            {
                fwrite(file_buffer, 1, read_size, storage_file);
            }
            fclose(file);
            fclose(storage_file);

            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "%s disponible au téléchargement avec /download %s\n", file_path, file_path);
            broadcast_message(channel, message, client_socket);
            continue;
        }

        if (strncmp(buffer, "/download ", 10) == 0)
        {
            char file_name[BUFFER_SIZE];
            sscanf(buffer + 10, "%s", file_name);

            char file_path[BUFFER_SIZE];
            snprintf(file_path, sizeof(file_path), "storage_server/storage_%d/%s", port, file_name);

            FILE *file = fopen(file_path, "rb");
            if (file == NULL)
            {
                send(client_socket, "Erreur téléchargement du fichier\n", 30, 0);
                continue;
            }
            fclose(file);

            send_file(client_socket, file_path);
            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "Téléchargement de %s réussi\n", file_name);
            send(client_socket, message, strlen(message), 0);
            continue;
        }

        if (strcmp(buffer, "/list") == 0)
        {
            list_files(client_socket, port);
            continue;
        }

        char formatted_message[BUFFER_SIZE];
        snprintf(formatted_message, sizeof(formatted_message), "%s : %s\n", client_name, buffer);

        log_message(port, client_name, buffer, channel_name);
        broadcast_message(channel, formatted_message, client_socket);
    }

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

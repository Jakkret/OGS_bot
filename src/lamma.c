/*
 * Jednoplikowa implementacja bota dla Gadu-Gadu w języku C
 * Projekt powstał na rzecz projektu OpenGaduServer
 *
 * środowisko: Windows Only
 * Data: 26.08.2026
 * Autor: Jakkret
 * 
 */ 

// namiary do serwera GG (zmieniasz przy kompilacji)

#define GG_SERVER       "10.0.1.20"
#define GG_PORT         8074

// koniec

#include <stdint.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

SOCKET g_sock;

#define UIN                   100
#define PASSWORD       "password"
#define GG_VERSION           0x29

#define BUFSIZE              1024


// --- Typy pakietów ---
#define GG_WELCOME      0x0001
#define GG_LOGIN50      0x000C
#define GG_LOGIN_OK     0x0003
#define GG_LOGIN_FAILED 0x0009
#define GG_LIST_EMPTY   0x0012
#define GG_SEND_MSG     0x000B
#define GG_RECV_MSG     0x000A
#define GG_PING         0x0008
#define GG_STATUS_AVAIL 0x0002

void gg_send_msg(SOCKET sock, uint32_t recipient, const char *text);

// --- Struktury ---
#pragma pack(push, 1)
typedef struct {
    uint32_t type;
    uint32_t length;
} gg_header_t;

typedef struct {
    uint32_t seed;
} gg_welcome_t;

typedef struct {
    uint32_t uin;
    uint32_t hash;
    uint32_t status;
    uint32_t version;
    uint32_t local_ip;
    uint16_t local_port;
} gg_login5_t;

typedef struct {
    uint32_t recipient;
    uint32_t seq;
    uint32_t msgclass;
} gg_send_msg_t;

typedef struct {
    uint32_t sender;
    uint32_t seq;
    uint32_t time;
    uint32_t msgclass;
} gg_recv_msg_t;
#pragma pack(pop)

uint32_t gg_hash(const uint8_t *password, uint32_t seed) {
    uint32_t x, y, z;
    y = seed;
    for (x = 0; *password; password++) {
        x = (x & 0xffffff00) | *password;
        y ^= x;
        y += x;
        x <<= 8;
        y ^= x;
        x <<= 8;
        y -= x;
        x <<= 8;
        y ^= x;
        z = y & 0x1f;
        y = (y << z) | (y >> (32 - z));
    }
    return y;
}

// Wyślij pakiet GG
int gg_send(SOCKET sock, uint32_t type, const void *data, uint32_t len) {
    gg_header_t header;
    header.type   = type;
    header.length = len;

    // wyślij nagłówek
    if (send(sock, (const char*)&header, sizeof(header), 0) <= 0)
        return -1;

    // wyślij dane jeśli są
    if (data && len > 0) {
        if (send(sock, (const char*)data, len, 0) <= 0)
            return -1;
    }

    return 0;
}

// Odbierz pakiet GG - zwraca typ pakietu, wypełnia body i body_len
// body trzeba zwolnić przez free() po użyciu!
int gg_recv(SOCKET sock, void **body, uint32_t *body_len) {
    gg_header_t header;
    int received;

    // odbierz nagłówek
    received = recv(sock, (char*)&header, sizeof(header), 0);
    if (received <= 0)
        return -1;

    *body_len = header.length;
    *body     = NULL;

    // odbierz dane jeśli są
    if (header.length > 0) {
        *body = malloc(header.length);
        if (!*body) return -1;

        received = recv(sock, (char*)*body, header.length, 0);
        if (received <= 0) {
            free(*body);
            *body = NULL;
            return -1;
        }
    }

    return (int)header.type;
}

void handle_command(SOCKET sock, uint32_t sender, const char *text) {
    if (strcmp(text, "/hello") == 0) {
        gg_send_msg(sock, sender, "Hej! Jestem botem GG. Napisz /help zeby zobaczyc komendy.");

    } else if (strcmp(text, "/help") == 0) {
        gg_send_msg(sock, sender,
            "Dostepne komendy:\r\n"
            "/hello - przywitaj sie\r\n"
            "/ping  - sprawdz czy bot zyje\r\n"
            "/info  - informacje o bocie\r\n"
            "/bye   - pozegnaj sie"
        );

    } else if (strcmp(text, "/ping") == 0) {
        gg_send_msg(sock, sender, "Pong!");

    } else if (strcmp(text, "/info") == 0) {
        gg_send_msg(sock, sender,
            "OpenGaduServer Test Bot\r\n"
            "Protokol: GG 5.x\r\n"
            "Napisany w C"
        );

    } else if (strcmp(text, "/bye") == 0) {
        gg_send_msg(sock, sender, "Do widzenia!");
    }
    // nieznana komenda lub zwykla wiadomosc - ignoruj
}


void gg_send_msg(SOCKET sock, uint32_t recipient, const char *text) {
    uint32_t  text_len = strlen(text) + 1;
    uint32_t  out_len  = sizeof(gg_send_msg_t) + text_len;
    uint8_t  *out      = malloc(out_len);
    if (!out) return;

    gg_send_msg_t *msg = (gg_send_msg_t*)out;
    msg->recipient = recipient;
    msg->seq       = (uint32_t)time(NULL);
    msg->msgclass  = 0x0004;
    memcpy(out + sizeof(gg_send_msg_t), text, text_len);

    gg_send(sock, GG_SEND_MSG, out, out_len);
    free(out);
}



int main(int argc, char *argv[]) {

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(GG_PORT);
    server_addr.sin_addr.s_addr = inet_addr(GG_SERVER);

    connect(g_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));


    // ODBIERANIE GG_WELCOME
    void *body = NULL;
    uint32_t body_len = 0;
    int type = gg_recv(g_sock, &body, &body_len);
    if (type != GG_WELCOME) {
        printf("Błąd: nie otrzymano ziarna. otrzymano typ %d\n", type);
        closesocket(g_sock);
        WSACleanup();
        return -1;
    }
    uint32_t seed = ((gg_welcome_t*)body)->seed;
    free(body);

    // WYSYŁANIE GG_LOGIN (wersja 5.0)
    gg_login5_t login;
    memset(&login, 0, sizeof(login));
    login.uin        = UIN;
    login.hash       = gg_hash(PASSWORD, seed);
    login.status     = GG_STATUS_AVAIL;
    login.version    = GG_VERSION;
    login.local_ip   = 0;
    login.local_port = 0;

    gg_send(g_sock, GG_LOGIN50, &login, sizeof(login));
    printf("wysłano GG_LOGIN\n", login.uin, login.hash);

    // ODBIERANIE C.D. LOGOWANIA
    type = gg_recv(g_sock, &body, &body_len);
    if (type == GG_LOGIN_OK) {
        printf("[OK] Zalogowano pomyślnie!\n");
    } else if (type == GG_LOGIN_FAILED) {
        printf("[ERR] Logowanie nieudane!\n");
        if (body) free(body);
        WSACleanup();
        return 1;
    } else {
        printf("[ERR] Nieoczekiwany pakiet: 0x%04X\n", type);
        if (body) free(body);
        WSACleanup();
        return 1;
    }
    if (body) { free(body); body = NULL; }

    // PUSTA LISTA KONTAKTÓW
    gg_send(g_sock, GG_LIST_EMPTY, NULL, 0);
    printf("[OK] GG_LIST_EMPTY wysłany\n");


    // --- Wyślij wiadomość testową ---
    const char *text    = "Hej, to jest testowa wiadomosc!\0";
    uint32_t    text_len = strlen(text) + 1;
    uint32_t    out_len  = sizeof(gg_send_msg_t) + text_len;
    uint8_t    *out      = malloc(out_len);

    gg_send_msg_t *msg = (gg_send_msg_t*)out;
    msg->recipient = UIN;   // wyślij do siebie
    msg->seq       = 1;
    msg->msgclass  = 0x0004;
    memcpy(out + sizeof(gg_send_msg_t), text, text_len);

    gg_send(g_sock, GG_SEND_MSG, out, out_len);
    printf("[OK] Wiadomosc testowa wysłana\n");
    free(out);

    // --- Główna pętla odbioru ---
    printf("[OK] Czekam na pakiety...\n");
    while (1) {
        type = gg_recv(g_sock, &body, &body_len);
        if (type < 0) {
            printf("[INFO] Połączenie zerwane\n");
            break;
        }

        switch (type) {
            case GG_RECV_MSG: {
                if (body_len < sizeof(gg_recv_msg_t)) break;
                gg_recv_msg_t *r = (gg_recv_msg_t*)body;
                const char *txt  = (const char*)body + sizeof(gg_recv_msg_t);
                printf("[MSG] Od UIN %u: %s\n", r->sender, txt);

                // obsługa komend
                handle_command(g_sock, r->sender, txt);
                break;
            }
            case GG_PING:
                gg_send(g_sock, GG_PING, NULL, 0);
                printf("[INFO] Pong!\n");
                break;
            default:
                printf("[INFO] Pakiet 0x%04X len=%u\n", type, body_len);
                break;
        }

        if (body) { free(body); body = NULL; }
    }

    WSACleanup();
    return 0;
}
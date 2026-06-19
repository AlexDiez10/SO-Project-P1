#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "ipc_common.h"
#include "types.h"

static const char *EMAIL_DICT[] = {
    "Thank", "Please", "Regards", "Meeting", "Attached",
    "Information", "Update", "Schedule", "Team", "Project", NULL
};
static const char *SCIENCE_DICT[] = {
    "Data", "Analysis", "Results", "Method", "Study",
    "Model", "Research", "System", "Significant", "Effect", NULL
};
static const char *REPORT_DICT[] = {
    "System", "Data", "Network", "Security", "Application",
    "Server", "User", "Performance", "Service", "Infrastructure", NULL
};


static BufferPool g_pool;
static UserClassifier g_classifier;
static pthread_mutex_t g_class_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_total_processes = 0;
static int g_done_processes = 0;

void        *client_thread(void *arg);
SentenceBuffer *find_or_create_buffer(pid_t pid, int sock_fd);
void         process_character(SentenceBuffer *buf, char c);
WordFreq     classify_document(SentenceBuffer *buf);
void         update_classifier(WordFreq *wf);
void         print_user_type(void);
int          count_hits(const char *sentence, const char **dict);
int          count_score(const char *sentence, const char **dict);

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fprintf(stderr, "In use: %s <num_process>\n", argv[0]);
        return 1;
    }
    g_total_processes = atoi(argv[1]);
    
    if(g_total_processes <= 0 || g_total_processes > MAX_PROCESSES)
    {
        fprintf(stderr, "num_processes must be 1 at %d\n", MAX_PROCESSES);
        return 1;
    }

    memset(&g_pool, 0, sizeof(g_pool));
    memset(&g_classifier, 0, sizeof(g_classifier));
    pthread_mutex_init(&g_pool.lock, NULL);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0) {perror("Socket"); return 1; }

    int opt = 1;

    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_address;
    memset(&server_address, 0,sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(IALEARNER_PORT);

    if(bind(server_sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) 
    {
        perror("Bind Server"); return 1;
    }

    if(listen(server_sock, MAX_PROCESSES) < 0) {perror("Listen Server"); return 1;}

    printf("IALEARNER listening in port %d...\n", IALEARNER_PORT);

int connected = 0;
    while(connected < g_total_processes)
    {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);

        int client_sock = accept(server_sock, (struct sockaddr *)&client_address, &client_len);

        if(client_sock < 0){perror("Accept Client"); continue;}

        int *sock_ptr = malloc(sizeof(int));
        if(!sock_ptr) {close(client_sock); continue;}
        *sock_ptr = client_sock;

        pthread_t tid;
        if(pthread_create(&tid, NULL, client_thread, sock_ptr) != 0)
        {
            perror("pthread_create");
            free(sock_ptr);
            close(client_sock);
            continue;;
        }

        pthread_detach(tid);
        connected++;

    }

    while(g_done_processes < g_total_processes) sleep(1);

    print_user_type();
    
    close(server_sock);
    return 0;

}

void *client_thread(void *arg)
{
    int sock = *(int *)arg;
    free(arg);

    SentenceBuffer *buf = NULL;

    while(1)
    {
        KeyPacket packet;
        int rc= recv(sock, &packet, sizeof(packet), 0);

        if(rc <= 0) break;

        if(buf == NULL)
        {
            buf = find_or_create_buffer(packet.process_id, sock);
            if(!buf) break;
        }

        if(packet.msg_type == MSG_PROC_DONE) break;

        process_character(buf, packet.character);
    }

    if(buf != NULL)
    {
        WordFreq wf = classify_document(buf);
        update_classifier(&wf);

        printf("[PID %d] clase: %s (email=%d sci=%d rep=%d)\n",
               buf->process_id,
               wf.assigned_class == DOC_EMAIL   ? "email"    :
               wf.assigned_class == DOC_SCIENCE  ? "ciencia"  :
               wf.assigned_class == DOC_REPORT   ? "reporte"  : "desconocido",
               wf.email_score, wf.science_score, wf.report_score);
    }

    pthread_mutex_lock(&g_class_lock);
    g_done_processes++;
    pthread_mutex_unlock(&g_class_lock);

    close(sock);
    return NULL;
}

SentenceBuffer *find_or_create_buffer(pid_t pid, int sock)
{
    pthread_mutex_lock(&g_pool.lock);

    for(int i = 0; i < g_pool.count; i++)
    {
        if(g_pool.buffers[i].process_id == pid)
        {
            pthread_mutex_unlock(&g_pool.lock);
            return &g_pool.buffers[i];
        }
    }

    if(g_pool.count >= MAX_PROCESSES)
    {
        pthread_mutex_unlock(&g_pool.lock);
        return NULL;
    }

    SentenceBuffer *buf = &g_pool.buffers[g_pool.count++];
    memset(buf, 0, sizeof(*buf));
    buf->process_id = pid;
    buf->socket_fd = sock;
    buf->is_active = 1;
    buf->sentences = malloc(sizeof(char *) * MAX_SENTENCES);
    buf->sent_capacity = MAX_SENTENCES;
    pthread_mutex_init(&buf->lock, NULL);

    pthread_mutex_unlock(&g_pool.lock);
    return buf;
}

void process_character(SentenceBuffer *buf, char c)
{
    pthread_mutex_lock(&buf->lock);

    if(c == '\n')
    {
        if(buf->cur_len > 0) {
            buf->current_sentence[buf->cur_len] = '\0';

            if(buf->sent_count < buf->sent_capacity)
            {
                buf->sentences[buf->sent_count] = strdup(buf->current_sentence);
                buf->sent_count++;
            }
            buf->cur_len = 0;
        }
    } else {
        if(buf->cur_len < MAX_SENTENCE_LEN - 1){
            buf->current_sentence[buf->cur_len++] = c;
        }
    }

    pthread_mutex_unlock(&buf->lock);
}

/* BAG OF WORDS */
int count_hits(const char *sentence, const char **dict)
{
    int hits = 0;
    for(int i = 0; dict[i] != NULL; i++)
    {
        if(strstr(sentence, dict[i]) != NULL) hits++;
    }
    return hits;
}

int count_score(const char *sentence, const char **dict)
{
    int score = 0;
    char *copy = strdup(sentence);
    char *token = strtok(copy, " \t\r\n");

    while (token != NULL)
    {
        for(int i = 0; dict[i] != NULL; i++)
        {
            if(strcmp(token, dict[i]) == 0) score++;
        }
        token = strtok(NULL, " \t\r\n");
    }
    free(copy);
    return score;
}

WordFreq classify_document(SentenceBuffer *buf)
{
    WordFreq wf;
    memset(&wf, 0, sizeof(wf));
    wf.process_id = buf->process_id;

    /* Procesar cada oración acumulada */
    for (int i = 0; i < buf->sent_count; i++) {
        const char *s = buf->sentences[i];

        wf.email_hits   += count_hits(s, EMAIL_DICT);
        wf.science_hits += count_hits(s, SCIENCE_DICT);
        wf.report_hits  += count_hits(s, REPORT_DICT);

        wf.email_score   += count_score(s, EMAIL_DICT);
        wf.science_score += count_score(s, SCIENCE_DICT);
        wf.report_score  += count_score(s, REPORT_DICT);
    }

    /* Regla: necesita al menos 3 hits para calificar */
    int email_ok   = (wf.email_hits   >= 3);
    int science_ok = (wf.science_hits >= 3);
    int report_ok  = (wf.report_hits  >= 3);

    if (!email_ok && !science_ok && !report_ok) {
        wf.assigned_class = DOC_UNKNOWN;
        return wf;
    }

    /* Desempate por mayor score */
    int best_score = -1;
    wf.assigned_class = DOC_UNKNOWN;

    if (email_ok && wf.email_score > best_score) {
        best_score = wf.email_score;
        wf.assigned_class = DOC_EMAIL;
    }
    if (science_ok && wf.science_score > best_score) {
        best_score = wf.science_score;
        wf.assigned_class = DOC_SCIENCE;
    }
    if (report_ok && wf.report_score > best_score) {
        wf.assigned_class = DOC_REPORT;
    }

    return wf;
}

void update_classifier(WordFreq *wf)
{
    pthread_mutex_lock(&g_class_lock);
    g_classifier.total_docs++;
    if (wf->assigned_class == DOC_EMAIL)    g_classifier.email_docs++;
    if (wf->assigned_class == DOC_SCIENCE)  g_classifier.science_docs++;
    if (wf->assigned_class == DOC_REPORT)   g_classifier.report_docs++;
    pthread_mutex_unlock(&g_class_lock);
}

void print_user_type(void)
{
    int t = g_classifier.total_docs;
    if (t == 0) { printf("Sin documentos clasificados.\n"); return; }

    int has_email   = g_classifier.email_docs   > 0;
    int has_science = g_classifier.science_docs > 0;
    int has_report  = g_classifier.report_docs  > 0;

    /* Tabla del proyecto:
       admin    → solo email
       técnico  → email + reporte
       profesor → email + ciencia
       estudiante → ciencia + reporte                */
    const char *user_type = "Desconocido";

    if (has_email && !has_science && !has_report)  user_type = "Personal administrativo";
    else if (has_email && has_report && !has_science) user_type = "Personal técnico";
    else if (has_email && has_science && !has_report) user_type = "Profesor";
    else if (has_science && has_report && !has_email) user_type = "Estudiante";

    printf("\n═══ Contexto del usuario ═══\n");
    printf("Documentos: email=%d  ciencia=%d  reporte=%d  total=%d\n",
           g_classifier.email_docs,
           g_classifier.science_docs,
           g_classifier.report_docs,
           t);
    printf("Tipo de usuario: %s\n", user_type);
}
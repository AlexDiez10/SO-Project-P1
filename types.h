#ifndef TYPES_H
#define TYPES_H

#include <sys/types.h>
#include <pthread.h>
#include "ipc_common.h"

/* 
*    LAUNCHER 
*/
typedef enum {
    PROC_RUNNING = 0,
    PROC_FINISHED,
    PROC_ERROR
} ProcessStatus;

typedef struct
{
    pid_t pid;
    int window_id;
    ProcessStatus status;
    char name[64];
    time_t start_time;
    time_t end_time;
    int exit_code;
} ProcessInfo;

typedef struct
{
    ProcessInfo entries[MAX_PROCESSES];
    int count;
    int alive;
    pthread_mutex_t lock;
} ProcessTable;


/* 
*    IALEARNER 
*/
typedef struct
{
    pid_t process_id;
    int socket_fd;

    char current_sentence[MAX_SENTENCE_LEN];
    int cur_len;

    char** sentences;
    int sent_count;
    int sent_capacity;

    int is_active;
    pthread_mutex_t lock;
} SentenceBuffer;

typedef struct
{
    SentenceBuffer buffers[MAX_PROCESSES];
    int count;
    pthread_mutex_t lock;
} BufferPool;

/* 
*    CLASIFICADOR 
*/

typedef enum
{
    DOC_UNKNOWN = 0,
    DOC_EMAIL = 1,
    DOC_SCIENCE = 2,
    DOC_REPORT = 3
} DocClass;

typedef struct
{
    pid_t process_id;

    /* frecuencias brutas: cuántas veces apareció cada palabra del diccionario */
    int email_score;
    int science_score;
    int report_score;

    /* hits distintos: cuántas PALABRAS DIFERENTES del diccionario aparecieron */
    int email_hits;
    int science_hits;
    int report_hits;

    DocClass assigned_class;
} WordFreq;

typedef enum
{
    USER_UNKNOWN   = 0,
    USER_ADMIN     = 1,
    USER_TECHNICAL = 2,
    USER_PROFESSOR = 3,
    USER_STUDENT   = 4
} UserType;

typedef struct
{
    int total_docs;
    int email_docs;
    int science_docs;
    int report_docs;

    float email_ratio;
    float science_ratio;
    float report_ratio;

    UserType result;
} UserClassifier;

#endif 

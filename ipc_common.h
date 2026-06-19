#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#include <sys/types.h>

/* Conf de red */
#define IALEARNER_PORT 9090
#define IALEARNER_HOST "127.0.0.1"

/* Tamaños */
#define MAX_SENTENCE_LEN 512
#define MAX_SENTENCES 1024
#define MAX_PROCESSES 64

/* PAquete que viaja por el socket */
typedef struct
{
    pid_t process_id;
    int window_id;
    char character;
    char msg_type;
    char _pad[2];
} KeyPacket;

/* Señales de control entre el LAuncher y IALearner */
#define MSG_KEY 'K'
#define MSG_PROC_DONE 'D'
#define MSG_ALL_DONE 'A'

#endif

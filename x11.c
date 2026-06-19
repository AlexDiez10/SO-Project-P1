#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include "ipc_common.h"

int connect_to_ialearner(void);

int main(int argc, char *argv[]) {

    int window_id = (argc > 1) ? atoi(argv[1]) : 0;

    int sock = connect_to_ialearner();
    if (sock < 0) return 1;

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        close(sock);
        return 1;
    }

    int screen = DefaultScreen(display);

    char title[32];
    snprintf(title, sizeof(title), "Agentic-OS window %d", window_id);

    Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        10 + window_id * 30, /* offset para evitar que se apilen */
        10 + window_id * 30, 
        400, 200,
        1,
        BlackPixel(display, screen),
        WhitePixel(display, screen)
    );

    XStoreName(display, window, title);

    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete, 1);

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

    KeyPacket packet;

    packet.window_id = window_id;
    packet.process_id = getpid();

    XEvent event;
    int running = 1;

    while (running) {
        XNextEvent(display, &event);

        if (event.type == KeyPress) {
            char buf[4] = {0};
            KeySym keysym;

            /* Obtiene el ASCII de la tecla */
            int len = XLookupString(&event.xkey, buf, sizeof(buf), &keysym, NULL);

            if (keysym == XK_Escape) running = 0;

            else if (keysym == XK_Return || keysym == XK_KP_Enter) {
                packet.character = '\n';
                packet.msg_type = MSG_KEY;
                send(sock, &packet, sizeof(packet), 0);

            } else if (len > 0 && buf[0] >= 32 && buf[0] < 127){
                packet.character = buf[0];
                packet.msg_type = MSG_KEY;
                send(sock, &packet, sizeof(packet), 0);
            }
            
        } else if (event.type == ClientMessage)
        {
            if((Atom)event.xclient.data.l[0] == wm_delete) running = 0;
        }
    }

    packet.character = '\0';
    packet.msg_type = MSG_PROC_DONE;
    send(sock, &packet, sizeof(packet), 0);

    close(sock);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}

int connect_to_ialearner(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(IALEARNER_PORT);

    if(inet_pton(AF_INET, IALEARNER_HOST, &address.sin_addr) <= 0)
    {
        perror("IP");
        close(sock);
        return -1;
    }

    if(connect(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Connection");
        close(sock);
        return -1;
    }

    return sock;
}
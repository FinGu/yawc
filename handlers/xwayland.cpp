#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <signal.h>

#include "../utils.hpp"
#include "../wm_defs.hpp"

int select_display_num(){
    if(access("/tmp/.X11-unix", F_OK) != 0){
        return 0;
    }

    int display_num = 0;

    char path[PATH_MAX];
    char lock_path[PATH_MAX];

    for(;;){
        snprintf(path, sizeof(path), "/tmp/.X11-unix/X%d", display_num);
        snprintf(lock_path, sizeof(lock_path), "/tmp/.X%d-lock", display_num);
        
        if(access(path, F_OK) != 0 && access(lock_path, F_OK) != 0){
            break;
        }

        if (display_num++ > 50) {
            return -1;
        }
    }

    return display_num;
}

int create_x11_socket(int display_num) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/.X11-unix/X%d", display_num);

    mkdir("/tmp/.X11-unix", 01777);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        wlr_log(WLR_ERROR, "Failed to bind X11 socket");
        close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0) {
        wlr_log(WLR_ERROR, "Failed to listen on X11 socket");
        close(fd);
        return -1;
    }

    return fd;
}

struct xwayland_data{
    yawc_server *server;
    int display_num;
    wl_listener destroy;
    pid_t pid;
};

void on_xwayland_destroy(yawc_server *server){
    auto *xdata = &server->xwayland_manager;
    char path[PATH_MAX];
    
    snprintf(path, sizeof(path), "/tmp/.X11-unix/X%d", xdata->display_num);
    unlink(path);

    snprintf(path, sizeof(path), "/tmp/.X%d-lock", xdata->display_num);
    unlink(path);

    if(xdata->startup){
        wl_event_source_remove(xdata->startup);
        xdata->startup = nullptr;
    }

    if(xdata->death){
        wl_event_source_remove(xdata->death);
        xdata->death = nullptr;
    }
}

int handle_death_event(int signal, void *data){
    yawc_server *server = static_cast<yawc_server*>(data);
    
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if(server->xwayland_manager.pid != pid){
            continue;
        }

        wlr_log(WLR_ERROR, "Xwayland crashed");

        on_xwayland_destroy(server);

        server->setup_xwayland();
    }

    return 1;
}

int handle_exec_xwayland(int fd, uint32_t mask, void *data){
    struct yawc_server *server = reinterpret_cast<struct yawc_server*>(data);
    auto *xdata = &server->xwayland_manager;
    
    int flags = fcntl(fd, F_GETFD);
    fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);

    std::string disp_str = ":" + std::to_string(xdata->display_num);

    pid_t pid = fork();
    
    if (pid == 0) {
        signal(SIGCHLD, SIG_DFL);
        
        execlp("xwayland-satellite", "xwayland-satellite", 
               disp_str.c_str(), 
               "-listenfd", 
               std::to_string(fd).c_str(),
               NULL);
               
        _exit(1);
    } else if (pid > 0) {
        xdata->pid = pid;
        wlr_log(WLR_INFO, "Started Xwayland (PID: %d)", pid);
    }
    
    close(fd);

    wl_event_source_remove(server->xwayland_manager.startup);
    server->xwayland_manager.startup = nullptr;

    return 0;
}

void yawc_server::setup_xwayland(){
    if (system("which xwayland-satellite > /dev/null 2>&1")) {
        wlr_log(WLR_INFO, "Starting without xwayland, couldn't find it in path");
        return;
    }

    int display_num = select_display_num();

    wlr_log(WLR_INFO, "Chose display num %d", display_num);

    int fd = create_x11_socket(display_num);

    this->xwayland_manager.startup = wl_event_loop_add_fd(this->wl_event_loop, fd, WL_EVENT_READABLE, 
            handle_exec_xwayland, this);

    this->xwayland_manager.death = wl_event_loop_add_signal(this->wl_event_loop, SIGCHLD, 
            handle_death_event, this);

    this->xwayland_manager.display_num = display_num;
    this->xwayland_manager.pid = -1;

    wlr_log(WLR_INFO, "Added the Xwayland event");
    
    std::string nd = ":" + std::to_string(display_num);
    
    setenv("DISPLAY", nd.c_str(), true);
}

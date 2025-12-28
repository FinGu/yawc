#include <getopt.h>
#include <dlfcn.h>

#include "server.hpp"

extern yawc_server *wm_server;

int main(int argc, char **argv){
    yawc_server server;

#ifdef DEBUG
    wlr_log_init(WLR_DEBUG, NULL);
#else 
    wlr_log_init(WLR_INFO, NULL);
#endif

    static struct option long_options[] ={
        {"help", no_argument, NULL, 'h'},
        {"window-manager", required_argument, NULL, 'w'},
        {"startup", required_argument, NULL, 's'},
        {"config", required_argument, NULL, 'c'},
        {0, 0, 0, 0}
    };

    char *wm_module_location, *startup_command, *custom_config_path; 
    wm_module_location = startup_command = custom_config_path = nullptr;

    int c, option_index;

    while((c = getopt_long(argc, argv, "hw:s:c:", long_options, &option_index)) != -1){
        switch(c){
            case 'w':
                wm_module_location = optarg;
                break;

            case 's': 
                startup_command = optarg;
                break;

            case 'c':
                custom_config_path = optarg;
                break;

            default:
            case 'h':
                wlr_log(WLR_INFO, "Help!");
                return 0;
        } 
    }

    server.wm = {0};

    yawc_config cfg;

    if(custom_config_path){
        cfg.load(custom_config_path);
    } else{
        char *home = getenv("XDG_CONFIG_HOME");

        std::string path = home ? home : "~/.config";

        if(!cfg.load(path + "/yawc.toml")){
            cfg.load("/etc/yawc/yawc.toml"); 
        }
    }
    
    wm_server = &server;
    server.config = &cfg;

    if(startup_command){
        server.config->autostart_cmds.push_back(startup_command);
    }

    if(wm_module_location){
        server.config->wm_path = wm_module_location;
    }

    if (server.run()) {
        wlr_log(WLR_ERROR, "Failed to start the compositor");
    }
    
    return 0;
}

#define NOB_IMPLEMENTATION
#include "nob.h"
bool cc(const char* ipath, const char* opath) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "gcc");
    nob_cmd_append(&cmd, "-g", "-c", ipath, "-o", opath);
    if(!nob_cmd_run_sync(cmd)) {
       nob_cmd_free(cmd);
       return false;
    }
    nob_cmd_free(cmd);
    return true;
}
bool ld() {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "gcc");
    nob_cmd_append(&cmd, 
        "./bin/main.o",
    );
    nob_cmd_append(&cmd, "-o", "slab");
    if(!nob_cmd_run_sync(cmd)) {
        nob_cmd_free(cmd);
        return false;
    }
    nob_cmd_free(cmd);
    return true;
}
bool build() {
    if(!cc("./src/main.c" , "./bin/main.o" )) return false;
    if(!ld()) return false;
    return true;
}
bool run() {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "./slab");
    if(!nob_cmd_run_sync(cmd)) {
       nob_cmd_free(cmd);
       return false;
    }
    nob_cmd_free(cmd);
    return true;
}

bool nob_mkdir_if_not_exists_silent(const char *path) {
     if(nob_file_exists(path)) return true;
     return nob_mkdir_if_not_exists(path);
}
int main(int argc, char** argv) {
   NOB_GO_REBUILD_URSELF(argc,argv);
   if(!nob_mkdir_if_not_exists_silent("./bin")) return 1;
   if(!build()) return 1;
   // if(!run()) return 1;
   return 0;
}

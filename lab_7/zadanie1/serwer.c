#include "biblioteka.h"
#define BACKLOG 3

int main(int argc, char** argv){
    if(argc != 3 ) usage(argv[0]);
    int local_socket = bind_local_socket(argv[1]);
}
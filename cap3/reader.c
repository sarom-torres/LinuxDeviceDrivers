#include <stdio.h>
#include <stdlib.h> //uso calloc
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

//mostrar em tela a string do erro 
int main(){
    
    char *path = "/dev/scull0"; 
    int fd, qtd, data_size = 8;
    
    fd = open(path,O_RDONLY);
    
    //calloc(numero de elementos,tamanho do elementos)
    char *buffer = (char*) calloc(100,sizeof(char));
    
    if(fd < 0){
        printf("Erro ao acessar dispositivo\n");
    }else{
        qtd = read(fd,buffer,data_size);
        if(qtd == -1){
            printf("Erro %d: %s\n",errno,strerror(errno));
        }else{
            buffer[qtd] = '\0';
            printf("Dados lidos: %s \n",buffer);
        }
    }
    
    return 0;
}

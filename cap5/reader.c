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
    
    char *path = "/dev/scull_fifo0"; 
    int fd, qtd, data_size = 4;
    int qtd2;
    fd = open(path,O_RDONLY);
    
    //calloc(numero de elementos,tamanho do elementos)
    char *buffer = (char*) calloc(100,sizeof(char));
    char *buffer2 = (char*) calloc(100,sizeof(char));
    
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
        sleep(5);
//         qtd2 = read(fd,buffer2,data_size);
//         if(qtd2 == -1){
//             printf("Erro %d: %s\n",errno,strerror(errno));
//         }else{
//             buffer2[qtd2] = '\0';
//             printf("Dados lidos: %s \n",buffer2);
//         }
    }
    free(buffer);
    free(buffer2);
    return 0;
}

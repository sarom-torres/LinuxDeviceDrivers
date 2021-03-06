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
    int fd, fd2, qtd, qtd2, size_msg1,size_msg2, size_read = 4;
    char *msg1 = "xy";
    char *msg2 = "qw";
    
    fd = open(path,O_WRONLY);
    fd2 = open(path,O_RDONLY);
    
    //calloc(numero de elementos,tamanho do elementos)
    char *buffer = (char*) calloc(100,sizeof(char));
    char *buffer2 = (char*) calloc(100,sizeof(char));
    
    if(fd < 0){
        printf("Erro ao acessar dispositivo\n");
    }else{
        //escrita
        size_msg1 = write(fd,msg1,strlen(msg1));
        
        if(size_msg1 < 0){
            printf("Erro %d: %s\n",errno,strerror(errno));
        }
        qtd = read(fd2,buffer,size_read);
        if(qtd == -1){
            printf("Erro %d: %s\n",errno,strerror(errno));
        }else{
            buffer[qtd] = '\0';
            printf("Dados lidos: %s \n",buffer);
        }

        size_msg2 = write(fd,msg2,strlen(msg2));

        if(size_msg2 < 0){
            printf("Erro %d: %s\n",errno,strerror(errno));
        }
        qtd2 = read(fd2,buffer2,size_read);
        if(qtd2 == -1){
            printf("Erro %d: %s\n",errno,strerror(errno));
        }else{
            buffer2[qtd2] = '\0';
            printf("Dados lidos: %s \n",buffer2);
        }
    }
    free(buffer);
    free(buffer2);    
    return 0;
}

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h> //funçoes do numero do módulo
#include <linux/types.h> // usado para o dev_t
#include <linux/cdev.h> //usado para alocar e registrar struct cdev
#include <linux/kernel.h> //contém a macro container_of
#include <linux/fcntl.h> //contém as f_flags
#include <linux/slab.h> //usada para o gerenciamento de memória
#include <asm/uaccess.h> //contém as funções copy_*_user
#include <linux/semaphore.h> //usada para os semáforos

#include "scull.h"

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
unsigned int scull_memory_max = SCULL_MEMORY_MAX;

struct scull_dev *scull_devices;

struct file_operations scull_fops = {
    //ponteiro para a propria estrutura, impede que o módulo seja descarregado enquanto está em uso
    .owner = THIS_MODULE,
    .read = scull_read,
    .write = scull_write,
    .open = scull_open,
    .release = scull_release,
};

MODULE_LICENSE("DuaL BSD/GPL");

//---------- CONFIG -----------------------------------------------------------------------
static void scull_setup_cdev(struct scull_dev *dev, int index){
    
    int err; 
    int devno = MKDEV(scull_major,scull_minor + index);
    
    //cdev representa o char device internamente
    //inicializando a estrutura cdev. 
    cdev_init(&dev->cdev,&scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_fops;
    
    err = cdev_add(&dev->cdev,devno,1);
    
    if(err) printk(KERN_NOTICE "Error %d adding scull%d",err,index);
    
}


//---------- READ e WRITE -----------------------------------------------------------------

//escreve os dados no buffer do dispositivo, retorna a quantia de dados gravados
ssize_t scull_recorder(struct scull_dev *dev, const char __user *buffer, size_t count,size_t gap){
    
    size_t retval = 0;
    
    char *buf = buffer+gap;
    

    if(dev->start < dev->end){
        int diff = scull_memory_max - dev->end; //diferenca entre o end e o limite do vetor
        if(count > diff){ 
            //se há necessidade de truncar os dados trunca e faz a realocação do ponteiro end 
            if(raw_copy_from_user(&dev->data[dev->end],buf,diff)){//copia a quantia que cabe na diferenca
                retval = -EFAULT;
                goto out;
            } 
            char * rest_data = buf+diff; //desloca o ponteiro para o restante dos dados a serem copiados
            dev->end = 0;
            
            if(raw_copy_from_user(&dev->data[dev->end],rest_data,(count-diff))){
                retval = -EFAULT;
                goto out;
            }
            dev->end += (count-diff);
        }else{
            //se não há necessidade de truncar os dados apenas copia
            if(raw_copy_from_user(&dev->data[dev->end],buf,count)){
                retval = -EFAULT;
                goto out;               
            }
            dev->end +=count;
        }
    }else{
        if(raw_copy_from_user(&dev->data[dev->end],buf,count)){
            retval = -EFAULT;
            goto out;
        }
        dev->end += count; 
    }
    
    dev->size += count;
    retval = count;
    
    out:
        return retval;
}



ssize_t scull_read(struct file *filp, char __user *buf,size_t count, loff_t *f_pos){
    
    struct scull_dev *dev = filp->private_data;
    ssize_t retval = 0;
    int flag = 0;
    
    //decrementa semáforo, se a operação é interrompida o valor de retorno é diferente de zero 
    //entao ele retorna o erro referente a uma chamada de sistema que é reiniciavél.
//     if(down_interruptible(&dev->sem)){
//         return -ERESTARTSYS;
//     }
    
    if(dev->size==0){
        retval = -EAGAIN;
        goto out;
    }
    
    if(count > dev->size)
        count = dev->size;
    
    if(dev->size == scull_memory_max) 
        flag++;
    
    if(raw_copy_to_user(buf,&dev->data[dev->start], count)){
        retval = -EFAULT;
        flag--;
        goto out;
    }
    
    memset(&dev->data[dev->start],0,count);
    
    dev->start += count;
    dev->size -= count;
    
    if(dev->start >= scull_memory_max)
        dev->start = 0;
    
    printk("--------------LEITURA-------------------");
    //printk("SCULL_READ : dev->buffer =  %s",buf);
    //printk("SCULL_READ : dev->size =  %d",dev->size);
    //printk("SCULL_READ : dev->start =  %d",dev->start);
    
    retval = count;
    
    //return retval;
    
    out:
        printk("SCULL_READ : out function");
        printk("SCULL_READ : dev->data =  %s",buf);
        if(flag > 0)up(&dev->sem);
        return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf,size_t count, loff_t *f_pos){
    
    struct scull_dev *dev = filp->private_data;
    ssize_t retval = 0, ret = 0;
    size_t new_count = 0; //quantia de dados que será gravada quando liberada a memória
    size_t gap = 0; //posicao no qual deve iniciar a próxima escrita 
    unsigned int cap = 0;//capacidade de memória livre 
    
    printk("--------------ESCRITA-------------------");
    
    cap = scull_memory_max - dev->size;//capacidade de memória livre 
    printk("SCULL_WRITE : capacidadeA %d ",cap);
    
    if(dev->size < scull_memory_max && (dev->size+count) < cap){
        retval = scull_recorder(dev,buf,count,gap); //grava
        printk("SCULL_WRITE : Entrou na < scull_memory_max");
    }else{    
        
        while(count != 0){ //enquanto existirem dados para gravar
            printk("SCULL_WRITE : Entrou no while");
            
            if(down_interruptible(&dev->sem)) return -ERESTARTSYS; //espera
            
            cap = scull_memory_max - dev->size; //capacidade de espaço livre após liberação de memoria
            printk("SCULL_WRITE : capacidadeB %d ",cap);
                
            if(count > cap) new_count = cap; // qtia de dados gravada será a capacidade da memória 
            else new_count = count; //prepara para gravar todos os dados restante
            printk("SCULL_WRITE : new_count %ld ",new_count);

            ret = scull_recorder(dev,buf,new_count,gap); //grava os dados na memória
                
            if(ret < 0){ //caso tenha dado algum erro ao gravar 
                retval = ret;
                goto out;
            }else{
                retval += ret;
            }
            gap += new_count; 
            count -= new_count;
            printk("SCULL_WRITE : count_final %ld ",count);
        }      

    }
    
    

    
    char *ptr = &dev->data[dev->start];

    printk("SCULL_WRITE : dev->ptrWrite =  %s",ptr);
    //printk("SCULL_WRITE : dev->size =  %d",dev->size);
    //printk("SCULL_WRITE : dev->end =  %d",dev->end);
    //return retval;
    
    out:
        printk("SCULL_WRITE : out function");
        //up(&dev->sem);
        return retval;
}

//---------- OPEN e RELEASE --------------------------------------------------------------
//struct file: representa um arquivo aberto
//struct inode: representa um arquivo no disco
int scull_open(struct inode *inode, struct file *filp){
    
    struct scull_dev *dev;
    
    /*Macro container_of usada para obtero endereço de uma estrutura 
     *que contém um campo do qual apenas conhecemos seu endereço
     *www.embarcados.com.br/conhecendo-a-macro-container_of/
     *container_of(ponteiro,container_type,container_field)
     */
    dev = container_of(inode->i_cdev,struct scull_dev,cdev);
    
    filp->private_data = dev;
    
    //if((filp->f_flags & O_ACCMODE) == O_WRONLY){
        //LIBERA MEMORIA
    //}
    
    return 0;
    
    
}

int scull_release(struct inode *inode,struct file *filp){
    return 0;
}

//---------- INIT e EXIT ----------------------------------------------------------------
static void __exit scull_exit(void){
    
    int i = 0;
    dev_t devno = MKDEV(scull_major,scull_minor);
    
    if(scull_devices){
        for(i=0;i<scull_nr_devs;i++){
            cdev_del(&scull_devices[i].cdev);
        }
        kfree(scull_devices);
    }
    
    unregister_chrdev_region(devno,scull_nr_devs);
    printk(KERN_ALERT "SCULL_EXIT : !Hasta la vista, baby!\n");
}

static int __init scull_init(void){
    
    int result, i;
    //tipo utilizado para representar device numbers no kernel
    dev_t dev = 0;
    
    if(scull_major){
        dev = MKDEV(scull_major,scull_minor);
        result = register_chrdev_region(dev,scull_nr_devs,"scull_fifo");
        printk(KERN_ALERT "SCULL_INIT : scull_major nonzero! Result: %d\n",result);
    }else{
        result = alloc_chrdev_region(&dev,scull_minor,scull_nr_devs,"scull_fifo");
        scull_major = MAJOR(dev);
        printk(KERN_ALERT "SCULL_INIT : Hi, I am T-800!");
        printk("SCULL_INIT : Major: %d\n",MAJOR(dev));
    }
    
    //caso a alocação seja feita com sucesso result = 0 caso não recebe negativo
    if(result < 0){
        printk(KERN_WARNING "SCULL_INIT : can't get major %d\n",scull_major);
        return result;
    }
    
    //alocando memória
    // o retorno da função é um ponteiro para a area de memória alocada
    scull_devices = kmalloc (scull_nr_devs*sizeof(struct scull_dev),GFP_KERNEL);
    
    if(!scull_devices){
        result = -ENOMEM;
        goto fail;
    }
    
    //função que preenche um bloco de memória com determinado valor
    //args: (ponteiro para o bloco de memoria, valor a ser escrito,numero de bytes)
    memset(scull_devices,0,scull_nr_devs*sizeof(struct scull_dev));
    
    for(i=0; i<scull_nr_devs;i++){
        scull_devices[i].size = 0;
        scull_devices[i].start = 0;
        scull_devices[i].end = 0;
        //semaforo usado para proteger a escrita e leitura
        sema_init(&scull_devices[i].sem,1);
        scull_setup_cdev(&scull_devices[i],i);
    }
       
    
    return 0;
    
    fail:
        scull_exit();
        return result;

}

module_init(scull_init);
module_exit(scull_exit);



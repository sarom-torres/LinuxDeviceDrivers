#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h> //funçoes do numero do módulo
#include <linux/types.h> // usado para o dev_t
#include <linux/cdev.h> //usado para alocar e registrar struct cdev
#include <linux/kernel.h> //contém a macro container_of
#include <linux/fcntl.h> //contém as f_flags
#include <linux/slab.h> //usada para o gerenciamento de memória

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


ssize_t scull_read(struct file *filp, char __user *buf,size_t count, loff_t *f_pos){
    return 0;
}

ssize_t scull_write(struct file *filp, const char __user *buf,size_t count, loff_t *f_pos){
    
    struct scull_dev * dev = filp->private_data;
    ssize_t retval = -ENOMEM;
    
    if(dev->size >= dev->memory){
        retval = -ENOMEM;
        goto out;
    }
    
    if(dev->size + count > dev->memory){
        retval = -ENOMEM;
        goto out;  
    }
    
    
    if(raw_copy_from_user(dev->data[f_pos],buf,count)){
        printk("SCULL_WRITE : não escreveu na memoria");
        retval = -EFAULT;
        goto out;
    }
    
    *f_pos += count;
    retval = count;
    dev->size += count;
    
    if(*f_pos > dev->memory){
        *f_pos = 0;
    }
    out:
        printk("SCULL_WRITE : out function");
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
    
    if((filp->f_flags & O_ACCMODE) == O_WRONLY){
        //LIBERA MEMORIA
    }
    
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
            //LIBERAR MEMORIA
            cdev_del(&scull_devices[i].cdev);
        }
        
    }
    
    unregister_chrdev_region(devno,scull_nr_devs);
    printk(KERN_ALERT "SCULL_EXIT : Goodbye, cruel world\n");
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
        printk(KERN_ALERT "SCULL_INIT : Hello, beautiful world!");
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
        printk("SCULL_INIT : Não está alocando memória");
        result = -ENOMEM;
        goto fail;
    }
    
    //função que preenche um bloco de memória com determinado valor
    //args: (ponteiro para o bloco de memoria, valor a ser escrito,numero de bytes)
    memset(scull_devices,0,scull_nr_devs*sizeof(struct scull_dev));
    
    for(i=0; i<scull_nr_devs;i++){
        scull_devices[i].memory = scull_memory_max;
        scull_devices[i].size = 0;
        scull_setup_cdev(&scull_devices[i],i);
    }
       
    
    return 0;
    
    fail:
        scull_exit();
        return result;

}

module_init(scull_init);
module_exit(scull_exit);

//     if(!dev->data){
//         printk("SCULL_WRITE : alocando primeiro data");
//         dev->data = kmalloc(sizeof(char),GFP_KERNEL);
//         if(dev->data == null){
//             printk("SCULL_WRITE : dev->data nulo");
//             return NULL;
//         }
//         memset(dev->data,0,sizeof(char));
//     }
// 
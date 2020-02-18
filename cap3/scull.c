#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h> //usada para as funções do numero do modulo
#include <linux/cdev.h> //usada para alocar e registrar estruturas do tipo struct cdev
#include <linux/slab.h> //usada para o gerenciamento de memória
#include <linux/kernel.h> //contém a macro container_of
#include <linux/fcntl.h> //contém as f_flags

#include "scull.h"


int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;


struct scull_dev *scull_devices; //representação interna do device | alocado no __scull_init 


MODULE_LICENSE("DuaL BSD/GPL");


struct file_operations scull_fops = {
    //ponteiro para a propria estrutura, impede que o módulo seja descarregado enquanto está em uso
    .owner = THIS_MODULE, 
    //usado para mudar a posição atual de leitura/escrita no file
    .llseek = scull_llseek,
    .read = scull_read,
    .write = scull_write,
    .open = scull_open,
    .release = scull_release,
};

//Libera toda a area de dados do scull_dev
int scull_trim(struct scull_dev *dev){
    
    struct scull_qset *next, *dptr;
    int qset = dev->qset;
    int i;
    
    //percorrendo cada scull_qset do scull_dev
    for(dptr=dev->data; dptr; dptr=next){
        
        if(dptr->data){
            for(i=0;i < qset;i++)
                kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }
    
    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;
    return 0;
}

// Configurando a estrutura char_dev para esse device
//## Não deveria ser um cdev no argumento ao inves de um scull_dev
static void scull_setup_cdev(struct scull_dev *dev, int index){
    int err, devno = MKDEV(scull_major, scull_minor + index); //##Pq fazer o MKDEV novamente com o idex?
    
    //cdev representa o char device internamente
    // inicializando a estrutura cdev. 
    cdev_init(&dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_fops;
    
    //informa ao kernel o device
    err = cdev_add(&dev->cdev,devno,1); //arg: (estrutura cdev, numero do device, quantia de devices que podem ser associados a ele)
    
    if(err) printk(KERN_NOTICE "Error %d adding scull%d",err,index);
    
    
}

ssize_t scull_read(struct file *filp, char __user *buf,size_t count, loff_t *f_pos);

ssize_t scull_write(struct file *filp, const char __user *buf,size_t count, loff_t *f_pos);

loff_t scull_llseek(struct file *filp, loff_t off, int whence);

long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

//ver se devo manter no .h

/*
 *scull_open faz a inicialização e preparação para operações posteriores
 *inode:  possui as informações no campo i_cdev que contem a struct cdev coonfigurada.
 */
int scull_open(struct inode *inode, struct file *filp){
    //informações do device
    struct scull_dev *dev; 
    
    /*Macro container_of usada para obtero endereço de uma estrutura 
     *que contém um campo do qual apenas conhecemos seu endereço
     *www.embarcados.com.br/conhecendo-a-macro-container_of/
     *container_of(ponteiro,container_type,container_field)
     */
    dev = container_of(inode->i_cdev,struct scull_dev,cdev);
    
    //armazena um ponteiro para scull_dev no campo private_data da estrutura file
    filp->private_data = dev;
    
    if((filp->f_flags & O_ACCMODE) == O_WRONLY){
        //libera toda area de dados quando o file é aberto para leitura
        scull_trim(dev);
    }
    return 0;
}
int scull_release(struct inode *inode, struct file *filp){
    return 0;
}

static void __exit scull_exit(void){
    
    dev_t devno = MKDEV(scull_major, scull_minor);
	
	//cancela o registro do major number
	unregister_chrdev_region(devno,scull_nr_devs);
	printk(KERN_ALERT "Goodbye, cruel world\n");
}

static int __init scull_init(void){
	int result,i;
    dev_t dev = 0;
    
	if (scull_major){
		dev = MKDEV(scull_major,scull_minor);
		result = register_chrdev_region(dev,scull_nr_devs,"scull");
		printk(KERN_ALERT "scull_major nonzero! Result: %d\n",result);
	}else{

		//faz a alocação dinâmica do major number
		result = alloc_chrdev_region(&dev,scull_minor,scull_nr_devs,"scull");
		scull_major = MAJOR(dev);
		printk(KERN_ALERT "scull_major zero! Result: %d\n",result);
		printk("Major: %d\n",MAJOR(dev));
		printk("Minor: %d\n",MINOR(dev));
	}

	//caso a alocação seja feita com sucesso result = 0 caso não recebe negativo
	if(result < 0){
		printk(KERN_WARNING "scull: can't get major %d\n",scull_major);
		return result;
	}

    //alocando memória
    // o retorno da função é um ponteiro para a area de memória alocada
    scull_devices = kmalloc(scull_nr_devs*sizeof(struct scull_dev),GFP_KERNEL);
    
    //Caso não tenha alocado memória
    if(!scull_devices){
        result = -ENOMEM;
        goto fail;        
    }
    
    //função que preenche um bloco de memória com determinado valor
    //args: (ponteiro para o bloco de memoria, valor a ser escrito,numero de bytes)
    memset(scull_devices,0,scull_nr_devs*sizeof(struct scull_dev));
    
    for(i=0;i<scull_nr_devs;i++){
        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].qset = scull_qset;
        sema_init(&scull_devices[i].sem,1);
        scull_setup_cdev(&scull_devices[i],i);
    }
    
    
    fail:
        scull_exit();
        return result;
	
	return 0;
}



module_init(scull_init);
module_exit(scull_exit);

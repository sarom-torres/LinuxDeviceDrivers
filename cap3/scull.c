#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h> //usada para as funções do numero do modulo
#include <linux/cdev.h> //usada para alocar e registrar estruturas do tipo struct cdev

#include "scull.h"


//SCULL_MAJOR está definida em scull.h
int scull_major = SCULL_MAJOR;
//int scull_major = 0;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
//int scull_nr_devs = 4;

//dev_t dev = 0; //##global tmp pq  precisava usar no registro. Está incorreto?

struct scull_dev * scull_devices; //alocado no __scull_init


MODULE_LICENSE("DuaL BSD/GPL");


struct file_operations scull_fops = {
    //ponteiro para a propria estrutura, impede que o módulo seja descarregado enquanto está em uso
    .owner = THIS_MODULE, 
    //usado para mudar a posição atual de leitura/escrita no file
    .llseek = scull_llseek,
    .read = scull_read,
    .write = scull_write,
    //para emitir comandos específicos do device
    .ioctl = scull_ioctl,
    .open = scull_open,
    .release = scull_release,
};

// Configurando a estrutura char_dev para esse device
//## Não deveria ser um cdev no argumento ao inves de um scull_dev
static void scull_setup_cdev(struct scull_dev *dev, int index){
    int err, devno = MKDEV(scull_major, scull_minor + index); //##Pq fazer o MKDEV novamente com o idex?
    
    //cdev representa o char device internamente
    // inicializando a estrutura cdev. 
    cdev_init(&dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.pos = &scull_fops;
    
    //informa ao kernel o device
    err = cdev_add(&dev->cdev,devno,1); //arg: (estrutura cdev, numero do device, quantia de devices que podem ser associados a ele)
    
    if(err) printk(KERN_NOTICE "Error %d adding scull%d",err,index);
    
    
}

static int __init scull_init(void){
	int result;
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

// FAZER A ALOCAÇÃO DO SCULL_DEVICES
	
	return 0;
}

static void __exit scull_exit(void){
    
    dev_t devno = MKDEV(scull_major, scull_minor);
	
	//cancela o registro do major number
	unregister_chrdev_region(devno,scull_nr_devs);
	printk(KERN_ALERT "Goodbye, cruel world\n");
}

module_init(scull_init);
module_exit(scull_exit);

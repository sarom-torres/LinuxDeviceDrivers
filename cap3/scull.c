#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h> //usada para as funções do numero do modulo
#include "scull.h"

//SCULL_MAJOR está definida em scull.h
int scull_major = SCULL_MAJOR;
//int scull_major = 0;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
//int scull_nr_devs = 4;
dev_t dev = 0;

MODULE_LICENSE("DuaL BSD/GPL");

// struct file_operations scull_fops = {
//     ponteiro para a propria estrutura, impede que o módulo seja descarregado enquanto está em uso
//     .owner = THIS_MODULE, 
//     usado para mudar a posição atual de leitura/escrita no file
//     .llseek = scull_llseek,
//     
// };

static int __init scull_init(void){
	int result;

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


	return 0;
}

static void __exit scull_exit(void){
	
	//cancela o registro do major number
	unregister_chrdev_region(dev,scull_nr_devs);
	printk(KERN_ALERT "Goodbye, cruel world\n");
}

module_init(scull_init);
module_exit(scull_exit);

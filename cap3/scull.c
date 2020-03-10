#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h> //usada para as funções do numero do modulo
#include <linux/cdev.h> //usada para alocar e registrar estruturas do tipo struct cdev
#include <linux/slab.h> //usada para o gerenciamento de memória
#include <linux/kernel.h> //contém a macro container_of
#include <linux/fcntl.h> //contém as f_flags
#include <linux/errno.h> //usada para os valores de erro
#include <asm/uaccess.h> //contém as funções copy_*_user


#include "scull.h"


int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;
int scull_memory = SCULL_MEMORY_MAX;


struct scull_dev *scull_devices; //representação interna do device | alocado no __scull_init 


MODULE_LICENSE("DuaL BSD/GPL");

//----------------------------------------------------------------------------------------- STRUCTS

struct file_operations scull_fops = {
    //ponteiro para a propria estrutura, impede que o módulo seja descarregado enquanto está em uso
    .owner = THIS_MODULE, 
    //usado para mudar a posição atual de leitura/escrita no file
    //.llseek = scull_llseek,
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
                // somente liberar se o quantum foi de fato alocado
                if (dptr->data[i]) kfree(dptr->data[i]);
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

//-------------------------------------------------------------------------------- CONFIGURAÇÕES

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
    //arg: (estrutura cdev, numero do device, quantia de devices que podem ser associados a ele)
    err = cdev_add(&dev->cdev,devno,1); 
    
    if(err) printk(KERN_NOTICE "Error %d adding scull%d",err,index);
    
    
}

//------------------------------------------------------------------------------- READ E WRITE

struct scull_qset *scull_follow(struct scull_dev *dev, int n){
    
    struct scull_qset *qs = dev->data;
    
    //Aloca o primeiro qset caso necessário
    if(!qs){
        printk("SCULL_FOLLOW : alocando o primeiro qset");
        qs = dev->data = kmalloc(sizeof(struct scull_qset),GFP_KERNEL);
        if(qs == NULL){
            printk("SCULL_FOLLOW : qset nulo");
            return NULL;
        }
        memset(qs,0,sizeof(struct scull_qset));
    }
    
    while(n--){
        if(!qs->next){
            qs->next = kmalloc(sizeof(struct scull_qset),GFP_KERNEL);
            if(qs->next == NULL){
                printk("SCULL_FOLLOW : qset->next nulo");
                return NULL;
            }
            memset(qs->next,0,sizeof(struct scull_qset));
        }
        qs = qs->next;
        continue;
    }
    return qs;
    
}


/*args: 
 * file: estrutura file
 * buf: ponteiro para o buffer do usuario que contém os dados
 * count: tamanho do dado para transferir
 * f_ops: indica a posição do arquivo que o usuário está acessando
 */
ssize_t scull_read(struct file *filp, char __user *buf,size_t count, loff_t *f_pos){
    
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr; //primeiro item da lista
    int quantum = dev->quantum, qset = dev->qset;
    int itemsize = quantum*qset; 
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;
    //int n_bytes = 0; // qtidade de bytes que não foram lidos
   
    //Caso não tenha dados para serem lidos retorna EAGAIN
    if(dev->size == 0){
        retval = -EAGAIN;
        goto out;
        
    }
    
    printk("SCULL_READ : dev_size = %ld",dev->size);
    
    //checagem para caso a posição do arquivo seja maior que o tamanho do device
    if(*f_pos >= dev->size)
        goto out;
    if(*f_pos + count > dev->size)
        count = dev->size - *f_pos;
    
    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;
    
    dptr = scull_follow(dev,item);
    
    if(dptr==NULL || !dptr->data || !dptr->data[s_pos]){
        goto out;
    }
    
    //somente leitura até o final deste quantum
    if(count > quantum - q_pos)
        count = quantum - q_pos;
    
    //raw_copy_to_user retorna a quantia de bytes que não foram copiados 
    //n_bytes = ;
    if(raw_copy_to_user(buf,dptr->data[s_pos] + q_pos, count)){
        // falta remover os valores que foram copiados em parte
        retval = -EFAULT;
        goto out;
    }
    else{
        printk("SCULL_READ : Liberando memoria");
//         struct scull_qset *ptr = memset(dptr->data,0,count);
//         kfree(ptr->data);
        memset(dptr->data[s_pos] + q_pos,0,count);
        //kfree(dptr->data);
    }
    
    *f_pos += count;
    //*f_pos = 0;
    retval = count;

    return retval;
    
    out:
        printk("SCULL_READ : out function");
        return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf,size_t count, loff_t *f_pos){
    
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum, qset = dev->qset;
    int itemsize = quantum *qset;
    int item, s_pos, q_pos, rest;
    ssize_t retval = -ENOMEM;
 //   int all_size = count;
    
    //Limita tamanho da memória
    if(dev->size >= scull_memory){
        retval = -ENOMEM;
        goto out;
    }
//     if(dev->size == scull_memory){
//         retval = all_size;
//         printk("SCULL_WRITE : Entrou aqui 2");
//         goto out;
//     }
    
    if((dev->size + count) > scull_memory){
        count = scull_memory - dev->size;
//        all_size -= count;
    }
    

    
    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;
    
    dptr = scull_follow(dev,item);
    
    if(dptr==NULL){
        printk("SCULL_WRITE : dptr é null");
        goto out;
    }
    
//*************************************************************************    
    if(!dptr->data){
        dptr->data = kmalloc(qset * sizeof(char *),GFP_KERNEL);
        if(!dptr->data){
            printk("SCULL_WRITE : kmalloc falhou");
            goto out;
        }
        memset(dptr->data,0,qset*sizeof(char *));
    }
//************************************************************************
    if(!dptr->data[s_pos]){
        printk("SCULL_WRITE : alocando quantum");
        dptr->data[s_pos] = kmalloc(quantum,GFP_KERNEL);
        if(!dptr->data[s_pos]){
            goto out;
        }
    }

    if(count > quantum - q_pos) count = quantum - q_pos;
  
    //O retorno é a quantidade de dados que ainda precisam ser copiadas caso exista falhas
    if(raw_copy_from_user(dptr->data[s_pos] + q_pos, buf, count)){
        printk("SCULL_WRITE : não escreveu na memória ");
        retval = -EFAULT;
        goto out;
    }
    
    *f_pos += count;
    retval = count;
    
    if(dev->size < *f_pos)
        dev->size  = *f_pos;
  
    return retval;
    
    out:
        printk("SCULL_WRITE : out function");
        return retval;
    
}

//loff_t scull_llseek(struct file *filp, loff_t off, int whence);



//----------------------------------------------------------------------------- OPEN E RELEASE
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

//------------------------------------------------------------------------------ EXIT E INIT
static void __exit scull_exit(void){
    
    int i;
    dev_t devno = MKDEV(scull_major, scull_minor);
    
    //liberando todas as entradas de char dev
    if(scull_devices){
        for(i=0;i<scull_nr_devs;i++){
            scull_trim(scull_devices +i);
            cdev_del(&scull_devices[i].cdev);
        }
        kfree(scull_devices);
    }
	
	//cancela o registro do major number
	unregister_chrdev_region(devno,scull_nr_devs);
	printk(KERN_ALERT "SCULL_EXIT : Goodbye, cruel world\n");
}

static int __init scull_init(void){
	int result,i;
    dev_t dev = 0;
    
	if (scull_major){
		dev = MKDEV(scull_major,scull_minor);
		result = register_chrdev_region(dev,scull_nr_devs,"scull");
		printk(KERN_ALERT "SCULL_INIT : scull_major nonzero! Result: %d\n",result);
	}else{

		//faz a alocação dinâmica do major number
		result = alloc_chrdev_region(&dev,scull_minor,scull_nr_devs,"scull");
		scull_major = MAJOR(dev);
		printk(KERN_ALERT "Hello, beautiful world!");
		printk("Major: %d\n",MAJOR(dev));
		printk("Minor: %d\n",MINOR(dev));
	}

	//caso a alocação seja feita com sucesso result = 0 caso não recebe negativo
	if(result < 0){
		printk(KERN_WARNING "SCULL_INIT : can't get major %d\n",scull_major);
		return result;
	}

    //alocando memória
    // o retorno da função é um ponteiro para a area de memória alocada
    scull_devices = kmalloc(scull_nr_devs*sizeof(struct scull_dev),GFP_KERNEL);
    
    //Caso não tenha alocado memória
    if(!scull_devices){
        printk("SCULL_INIT : Não está alocando memória");
        result = -ENOMEM;
        goto fail;        
    }
    
    //função que preenche um bloco de memória com determinado valor
    //args: (ponteiro para o bloco de memoria, valor a ser escrito,numero de bytes)
    memset(scull_devices,0,scull_nr_devs*sizeof(struct scull_dev));
    
    for(i=0;i<scull_nr_devs;i++){
        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].qset = scull_qset;
        //sema_init(&scull_devices[i].sem,1);
        scull_setup_cdev(&scull_devices[i],i);
    }
        
    return 0;
    
    fail:
        printk("SCULL_INIT : Fail function");
        scull_exit();
        return result;
	
	return 0;
}

//---------------------------------------------------------------------------- MODULE

module_init(scull_init);
module_exit(scull_exit);

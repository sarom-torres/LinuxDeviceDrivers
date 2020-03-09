#ifndef _SCULL_H_
#define _SCULL_H_


//----------------------- | Macros
//Major number
#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0
#endif

//Números de devices
#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 4
#endif


#ifndef SCULL_MEMORY_MAX
#define SCULL_MEMORY_MAX 4000
#endif


struct scull_dev{
    void **data; //ponteiro para o primeiro quantum sets
    unsigned int size; //Quantia de dados armazenados
    unsigned int memory;  //memoria ##faz sentido cada scull dev ter sua propria memoria ou deveria ser uma paratodo o código
    struct cdev cdev;
};


//---------------------- | Variáveis públicas
//foram declaradas em outro arquivo do código
extern int scull_major;
extern int scull_nr_devs;
//extern int scull_memory;


//----------------------- | Protótipos das funções

//int scull_p_init(dev_t dev);
//int scull_trim(struct scull_dev *dev);

ssize_t scull_read(struct file *filp, char __user *buf,size_t count, loff_t *f_pos);

ssize_t scull_write(struct file *filp, const char __user *buf,size_t count, loff_t *f_pos);

//loff_t scull_llseek(struct file *filp, loff_t off, int whence);

//ver se devo manter no .h
int scull_open(struct inode *inode, struct file *filp);
int scull_release(struct inode *inode, struct file *filp);

#endif


//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2018
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "memory_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

//Declaring a list to store task ids
struct task{
    struct task_struct* currTask;
    struct task *next;
};

struct object{
    unsigned long long int oid;
    unsigned long pfn;
    char* address;
    struct object *next;
};

//Declaring a list to store container ids and a pointer to associated task ids
struct container {
    unsigned long long int cid;
    struct task *task_list;
    struct object *object_list;
    struct mutex object_lock;
    // struct lock *lock_list;
    struct container *next;
}*container_head = NULL;

//Declaring a mutex variable
DEFINE_MUTEX(my_mutex);

//Adding a new container to the list of containers
//returns pointer to newly added container
struct container * addcontainer(struct container **head, unsigned long long int cid)
{
    struct container* temp = kmalloc( sizeof(struct container), GFP_KERNEL );
    if (temp == NULL)
    {
        printk("Not enough memory to add container : %llu", cid);
        return *head;
    }
    temp->cid = cid;
    temp->task_list = NULL;
    temp->object_list = NULL;
    mutex_init(&(temp->object_lock));
    // temp->lock_list = NULL;
    if(*head == NULL)
    {
        temp->next = *head;
        *head=temp;
    }
    else
    {
        struct container* temp2;
        temp2= *head;
        while(temp2->next)
                temp2=temp2->next;
        temp->next=temp2->next;
        temp2->next=temp;
    }
    return *head;
}

struct object * findobject(struct object *head, unsigned long long int oid)
{
    while(head)
    {
        if (head->oid == oid)
            return head;
        else
            head = head->next;
    }
    return NULL;
}

struct container * findcontainer(int pid)
{
    struct container *head;
    head = container_head;
    if (pid)
    {
        while (head)
        {
            struct task *task_head;
            task_head = head->task_list;
            while (task_head)
            {
                if (task_head->currTask->pid == pid)
                    return head;
                else
                    task_head = task_head->next;
            }
            head = head->next;
        }
        return NULL;
    }
    return NULL;
}

//Adding a new task to an already existing container's task list
//returns pointer to the head of the list
struct task * addtask(struct task **head, struct task_struct* currTask)
{
    struct task *temp = kmalloc( sizeof(struct task), GFP_KERNEL );
    if (temp == NULL)
    {
        printk("Not enough memory to add task : %d", currTask->pid);
        return *head;
    }    
        
    temp->currTask = currTask;
    if(*head == NULL)
    {
        temp->next = *head;
        *head=temp;
    }
    else
    {
        struct task* temp2;
        temp2= *head;
        while(temp2->next)
                temp2=temp2->next;
        temp->next=temp2->next;
        temp2->next=temp;
    }
    return *head;
}


struct object * addobject(struct object **head, unsigned long long int oid)
{
    struct object *temp = kmalloc( sizeof(struct object), GFP_KERNEL );
    if (temp == NULL)
    {
        printk("Not enough memory to add object : %d", oid);
        return *head;
    }    
        
    temp->oid = oid;
    temp->address = NULL;
    if(*head == NULL)
    {
        temp->next = *head;
        *head=temp;
    }
    else
    {
        struct object* temp2;
        temp2= *head;
        while(temp2->next)
                temp2=temp2->next;
        temp->next=temp2->next;
        temp2->next=temp;
    }
    return *head;
}


struct container * deletecontainer(struct container **head, unsigned long long int cid)
{
    struct container* temp_head, *prev;
    temp_head = *head;
    if (temp_head != NULL && temp_head->cid == cid) 
        { 
            *head = temp_head->next;   
            kfree(temp_head);         
            return *head; 
        }

    while (temp_head != NULL && temp_head->cid != cid) 
    { 
        prev = temp_head; 
        temp_head = temp_head->next; 
    } 
   
    if (temp_head == NULL) 
    {
        printk("\nContainer not found : %llu", cid);
        return *head;
    } 

    prev->next = temp_head->next; 
    kfree(temp_head);
    return *head;
}

struct task * deletetask(struct task **head, int pid)
{
    struct task* temp_head, *prev;
    temp_head = *head;
    if (temp_head != NULL && temp_head->currTask->pid == pid) 
        { 
            *head = temp_head->next;   
            kfree(temp_head);         
            return *head; 
        }

    while (temp_head != NULL && temp_head->currTask->pid != pid) 
    { 
        prev = temp_head; 
        temp_head = temp_head->next; 
    } 
   
    if (temp_head == NULL) 
    {
        printk("\nTask not found : %d", pid);
        return *head;
    } 
    
    prev->next = temp_head->next; 
    kfree(temp_head);
    return *head;
}

void display_obj_list(struct object *head){
    printk("\nDisplaying object list");
    while(head)
    {
        printk("\nObject OID -> %llu", head->oid);
        head = head->next;
    }
}

struct object * deleteobject(struct object **head, unsigned long long int oid)
{
    printk("\nInside delete object");
    display_obj_list(*head);
    struct object* temp_head, *prev;
    temp_head = *head;
    if (temp_head != NULL && temp_head->oid == oid) 
        { 
            *head = temp_head->next; 
            printk("\nObject to be freed found OID: %llu", oid);
            kfree(temp_head->address);
            temp_head->address = NULL;
            // printk("\nSet address pointer to NULL");  
            kfree(temp_head);
            printk("\nReturning object list");         
            display_obj_list(*head);
            return *head; 
        }
    while (temp_head != NULL && temp_head->oid != oid) 
    { 
        prev = temp_head; 
        temp_head = temp_head->next; 
    } 
   
    if (temp_head == NULL) 
    {
        printk("\nobject not found : %d", oid);
        return *head;
    } 
    
    prev->next = temp_head->next;
    printk("\nObject to be freed found OID: %llu", oid);
    kfree(temp_head->address);
    temp_head->address = NULL; 
    // printk("\nSet address pointer to NULL");  
    kfree(temp_head);
    printk("\nReturning object list");
    display_obj_list(*head);
    return *head;
}

void display_list(void)
{
    struct container *tc = container_head;
    while(tc)
    {
        struct task *tl = tc->task_list;
        while(tl)
        {
            printk("\n CID : %llu ----  PID : %d State: %d", tc->cid, tl->currTask->pid, tl->currTask->state);
            tl=tl->next;
        }
        struct object *ol = tc->object_list;
        while(ol)
        {
            printk("\n CID : %llu ----  OID : %d", tc->cid, ol->oid);
            ol=ol->next;
        }
        tc = tc->next;
    }
}

struct task * get_next_task(struct task **head, int pid)
{
    struct container *temp_container;
    temp_container = container_head;
    struct task* temp_task_head;
    int tflag = 0;

    while(temp_container)
    {
        temp_task_head = temp_container->task_list;
        while(temp_task_head)
        {
            if (temp_task_head->currTask->pid == pid)
                {
                    tflag = 1;
                    break;
                }
            temp_task_head=temp_task_head->next;
        }
        if (tflag)
            break;
        temp_container=temp_container->next;        
    }

    
    
    if (tflag)
    {    
        if (temp_task_head->next)
            return temp_task_head->next;
        else if (temp_container)
            return temp_container->task_list;
        else
            return NULL;
    }
    else
        return NULL;  
}

int memory_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
    mutex_lock(&my_mutex);
    struct vm_area_struct temp_vma;
    copy_from_user(&temp_vma, vma, sizeof(struct vm_area_struct));
    struct container *temp_container;
    int pid = current->pid;
    temp_container = findcontainer(pid);
    int object_size = vma->vm_end - vma->vm_start;
    unsigned long long int oid = vma->vm_pgoff;
    printk("\nInside mmap : PID -> %d --- OID -> %lu", pid, oid);
    printk("\n mmap start -> %lu --- end -> %lu", vma->vm_start, vma->vm_end);
    printk("\n mmap diff : %lu", vma->vm_end - vma->vm_start);
    int flag = 0;
    struct object *temp_object_list;
    if (temp_container)
    {
        temp_object_list = temp_container->object_list;
        if (temp_object_list)
        {
            struct object *existing_object = findobject(temp_object_list, oid);
            if (existing_object) 
            {        
                flag = 1;
                remap_pfn_range(vma, vma->vm_start, existing_object->pfn, object_size, vma->vm_page_prot);
                printk("\nObject exists: CID -> %llu --- PID -> %d --- OID: %llu", temp_container->cid, pid, oid);
            }
        }
        if (!flag)
        {
            struct object *curr_object;
            temp_object_list = addobject(&temp_object_list, oid);
            printk("\nCreating object : CID -> %llu --- PID -> %d --- OID: %llu", temp_container->cid, pid, oid);
            temp_container->object_list = temp_object_list;
            curr_object = findobject(temp_object_list, oid);
            if (curr_object)
            {
                printk("\nObject created successfully : CID -> %llu --- PID -> %d --- OID: %llu", temp_container->cid, pid, oid);
                char *memory_space = (char*) kmalloc((object_size)*sizeof(char), GFP_KERNEL );
                curr_object->address = memory_space;
                unsigned long pfn = virt_to_phys((void*)memory_space)>>PAGE_SHIFT;
                curr_object->pfn = pfn;
                int remaped = remap_pfn_range(vma, vma->vm_start, pfn, object_size,vma->vm_page_prot);
                printk("\nmmap done successfully");
                display_obj_list(temp_container->object_list);
                if (remaped < 0)
                {
                    printk("\n Can't remap CID -> %llu --- PID -> %d --- OID: %llu", temp_container->cid, pid, oid);
                }
            }
        }
    }
    else{
        printk("\nContainer with PID -> %d not found", pid);
    }
    mutex_unlock(&my_mutex);
    return 0;
}


int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
    mutex_lock(&my_mutex);
    struct memory_container_cmd temp_cmd;
    copy_from_user(&temp_cmd, user_cmd, sizeof(struct memory_container_cmd));
    //Setting calling thread's associated pid
    int pid = current->pid;
    //Setting calling thread's associated cid
    // unsigned long long int cid = temp_cmd.cid;
    unsigned long long int oid = temp_cmd.oid;
    struct container *temp_container;
    temp_container = findcontainer(pid);
    if (temp_container)
        printk("\nInside lock : CID -> %llu --- PID -> %d --- OID -> %llu", temp_container->cid, pid, oid);

    int flag =0;
    if (temp_container)
    {
        mutex_unlock(&my_mutex);
        mutex_lock(&(temp_container->object_lock));
    }
    else{
        printk("\nContainer with PID -> %d not found", pid);
        mutex_unlock(&my_mutex);
    }
    return 0;
}


int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{
    mutex_lock(&my_mutex);
    struct memory_container_cmd temp_cmd;
    copy_from_user(&temp_cmd, user_cmd, sizeof(struct memory_container_cmd));
    //Setting calling thread's associated pid
    int pid = current->pid;
    //Setting calling thread's associated cid
    // unsigned long long int cid = temp_cmd.cid;
    unsigned long long int oid = temp_cmd.oid;
    struct container *temp_container;
    temp_container = findcontainer(pid);
    if (temp_container)
        printk("\nInside unlock : CID -> %llu --- PID -> %d --- OID -> %llu", temp_container->cid, pid, oid);

    int flag =0;
    if (temp_container)
    {
        mutex_unlock(&my_mutex);
        mutex_unlock(&(temp_container->object_lock));
    }
    else{
        printk("\nContainer with PID -> %d not found", pid);
        mutex_unlock(&my_mutex);
    }
    return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
    mutex_lock(&my_mutex);
    struct memory_container_cmd temp_cmd;
    copy_from_user(&temp_cmd, user_cmd, sizeof(struct memory_container_cmd));
    
    //Setting calling thread's associated cid
    
    //Setting calling thread's associated pid
    int pid = current->pid;
    struct container *temp_container;
    temp_container = findcontainer(pid);
    unsigned long long int cid = temp_container->cid;
    printk("\nInside Delete : CID -> %llu --- PID -> %d", cid, pid);
    if(temp_container)
    {
    
        struct task *temp_task_head = temp_container->task_list;
        temp_task_head = deletetask(&temp_task_head, pid);
        printk("\n Task deleted : %d within Container : %llu", pid, cid); 
        temp_container->task_list = temp_task_head;
        // if (!temp_task_head)
        // {
        //     struct object *temp_object_list = temp_container->object_list;
        //     while(temp_object_list)
        //     {
        //         printk("\nDeleting object CID -> %llu --- OID -> %llu", cid, temp_object_list->oid);
        //         temp_object_list = deleteobject(&temp_object_list, temp_object_list->oid); 
        //     }
        //     container_head = deletecontainer(&container_head, cid);
        //     printk("\n Container Deleted : %llu", cid);
        // }
    }
    // printk("\nDeleting task : CID -> %llu --- PID -> %d", cid, pid);
    display_list();
    mutex_unlock(&my_mutex);
    return 0;
}


int memory_container_create(struct memory_container_cmd __user *user_cmd)
{
 
    //Mutex Lock
    mutex_lock(&my_mutex);

    struct memory_container_cmd temp_cmd;
    copy_from_user(&temp_cmd, user_cmd, sizeof(struct memory_container_cmd));
    
    //Setting calling thread's associated cid
    unsigned long long int cid = temp_cmd.cid;
    //Setting calling thread's associated pid
    int pid = current->pid;
    printk("\nInside Create : CID -> %llu --- PID -> %d", cid, pid);

    struct container *temp_container;
    temp_container = container_head;
    //Searching if a container is already present with the given cid
    //If yes add a new task to it's task list
    int flag = 0;
    while(temp_container)
    {
        if (temp_container->cid == cid)
        {
            struct task *task_head;
            task_head = temp_container->task_list;
            task_head = addtask(&task_head, current);
            temp_container->task_list = task_head;
            printk("\nExisting Container -> Creating task : CID -> %llu --- PID -> %d", cid, pid);
            flag = 1;
            break;
        }
        temp_container=temp_container->next;
    }    
    
    //Create a new container if container not present, and add the current task to it's task list
    //Keep this task awake in the container
    if (!flag)
    {
        container_head = addcontainer(&container_head, cid);    
        temp_container = container_head;
        while(temp_container)
        {
            if (temp_container->cid == cid)
            {    
                struct task *task_head;
                task_head = temp_container->task_list;   
                task_head = addtask(&task_head, current);
                temp_container->task_list = task_head;
                break;    
            }
            temp_container=temp_container->next;
        }
       printk("\n New container -> Creating task : CID -> %llu --- PID -> %d", cid, pid);
    }
    display_list();
    mutex_unlock(&my_mutex);
    return 0;
}


int memory_container_free(struct memory_container_cmd __user *user_cmd)
{
    mutex_lock(&my_mutex);
    struct memory_container_cmd temp_cmd;
    copy_from_user(&temp_cmd, user_cmd, sizeof(struct memory_container_cmd));
    //Setting calling thread's associated pid
    int pid = current->pid;
    //Setting calling thread's associated cid
    // unsigned long long int cid = temp_cmd.cid;
    unsigned long long int oid = temp_cmd.oid;
    struct container *temp_container;
    temp_container = findcontainer(pid);
    struct object *temp_object_list;

    if (temp_container)
        printk("\nInside Free : CID -> %llu --- PID -> %d --- OID -> %llu", temp_container->cid, pid, oid);
    if (temp_container)
    {
        temp_object_list = temp_container->object_list;
        if (temp_object_list)
        {
            temp_object_list = deleteobject(&temp_object_list, oid);
            temp_container->object_list = temp_object_list;
            printk("\nObject Deleted: CID -> %llu --- PID -> %d --- OID: %llu", temp_container->cid, pid, oid);
        }
        else{
            printk("\nObject list empty: CID -> %llu --- PID -> %d --- OID: %llu", temp_container->cid, pid, oid);
        }
    }
    else{
        printk("\nContainer with PID -> %d not found", pid);
    }
    mutex_unlock(&my_mutex);
    return 0;
}


/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int memory_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case MCONTAINER_IOCTL_CREATE:
        return memory_container_create((void __user *)arg);
    case MCONTAINER_IOCTL_DELETE:
        return memory_container_delete((void __user *)arg);
    case MCONTAINER_IOCTL_LOCK:
        return memory_container_lock((void __user *)arg);
    case MCONTAINER_IOCTL_UNLOCK:
        return memory_container_unlock((void __user *)arg);
    case MCONTAINER_IOCTL_FREE:
        return memory_container_free((void __user *)arg);
    default:
        return -ENOTTY;
    }
}

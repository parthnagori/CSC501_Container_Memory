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

//Declaring a list to store Object Ids
struct object{
    unsigned long long int oid;
    unsigned long PFNumber;
    char* address;
    struct object *next;
};

//Declaring list for Locks for Objects Ids
struct lock{
    unsigned long long int oid;
    struct mutex lockoid;
    struct lock *next;

};

//Declaring a list to store container ids and a pointer to associated task ids
struct container {
    unsigned long long int cid;
    struct container *next;
    struct task *task_list;
    struct object *object_list;
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
//        printk("Not enough memory to add container : %llu", cid);
        return *head;
    }
    temp->cid = cid;
    temp->task_list = NULL;
    temp->object_list = NULL;
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

struct container * findcontainer(int pid)
{
    struct container *head;
    head = container_head;

    if (cid)
    {
        while (head)
        {
            if (head->cid == cid)
                return head;
            else
                head = head->next;
        }
        return NULL;
    }
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
//        printk("Not enough memory to add task : %d", currTask->pid);
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


struct object * addobject(struct object **head, int oid)
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
//        printk("\nContainer not found : %llu", cid);
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
//        printk("\nTask not found : %d", pid);
        return *head;
    } 
    
    prev->next = temp_head->next; 
    kfree(temp_head);
    return *head;
}


struct object * deleteobject(struct object **head, int oid)
{
    struct object* temp_head, *prev;
    temp_head = *head;
    if (temp_head != NULL && temp_head->currObject->oid == oid) 
    {
        *head = temp_head->next;
        kfree(temp_head);
        return *head;
    }
    while(temp_head != NULL && temp_head->currObject->oid != oid)
    {
        prev = temp_head;
        temp_head = temp_head->next;
    }
    if (temp_head == NULL) 
    {
        return *head;
    }
    
    prev->next = temp_head->next; 
    kfree(temp_head);
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
    struct vm_area_struct temp_vma;
    struct container *temp_container;
    struct object *currObj;
    struct object *prevObj = NULL;
    struct object *newObj = NULL;
    unsigned long ObjSize;
    unsigned long long int currCid;

    struct vm_area_struct* mem_vma = (struct vm_area_struct*) kcalloc(1,sizeof(struct *), GFP_KERNEL);
    mutex_lock(&lock);
    copy_from_user(mem_vma, vma, sizeof(struct vm_area_struct));

    unsigned long long int curr_offset = mem_vma->vm_pgoff;
    ObjSize = mem_vma->vm_end - mem_vma->vm_start;
    
    kfree(mem_vma);
    mem_vma = NULL;
    int pid = current->pid;

    temp_container = findcontainer(pid);
    currCid = temp_container->cid
    currHead = container_head

    while (currCid != NULL) 
    {
        if (currHead->cid == currCid)
        {
            currObj = curr->object_list;
            while (currObj !=NULL)
            {
                if (currObj->oid == vma->vm_pgoff)
                {
                remap_pfn_range(vma, vma->vm_start, currObj->PFNumber, vma->vm_end-vma->vm_start, vma->vm_page_prot);
                break;
                }
                prevObj = currObj;
                currObj = currObj ->next;
            }
            if (currObj==NULL){
                char* reservedMem = (char*) kmalloc((vma->vm_end - vma->vm_start)*sizeof(char), GFP_KERNEL);
                newObj = (struct object*) kcalloc(1,sizeof(struct object), GFP_KERNEL);
                newObj -> address = reservedMem;
                newObj -> next = NULL;
                newObj -> oid = (unsigned long long int)vma->vm_pgoff;
                newObj -> PFNumber = virt_to_phys((void*)reservedMem)>>PAGE_SHIFT;
                remap_pfn_range(vma, vma->vm_start, currObj->PFNumber, vma->vm_end-vma->vm_start, vma->vm_page_prot);

                if(prevObj == NULL){
                    currHead->object_list = newObj;
                }
                else prevObj->next = newObj;
            }
            break;

        }
        currHead = currHead->next;

    }
    mutex_unlock(&lock)

    //Setting calling thread's associated cid
    // unsigned long long int cid = temp_vma.cid;
    // unsigned long long int oid = temp_vma.pgoffset;
    //Setting calling thread's associated pid

    // printk("\nInside mmap : PID -> %d --- OID -> %lu", pid, vma->vm_pgoff);
    // printk("\n mmap start -> %lu --- end -> %lu", vma->vm_start, vma->vm_end);
    // printk("\n mmap diff : %lu", );

    return 0;
}


int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
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
        if (temp_container->object_list)
        {
            struct object *temp_object;
            temp_object = temp_container->object_list;
            while(temp_object)
            {
                if (temp_object->oid == oid)
                { 
                    flag = 1;
                    printk("\nObject exists: CID -> %llu --- PID -> %d --- OID: %llu", temp_container->cid, pid, oid);
                    break;
                }
                else
                    temp_object = temp_object->next;
            }
        }
        else
        {
            flag = 0;
        }
        if (!flag)
        {
            struct object *object_head;
            object_head = temp_container->object_list;
            object_head = addobject(&object_head, oid);
            temp_container->object_list = object_head;
            printk("\nCreating object : CID -> %llu --- PID -> %d --- OID: %llu", temp_container->cid, pid, oid);
        }
    }
    return 0;
}


int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{
    struct memory_container_cmd temp_cmd;
    copy_from_user(&temp_cmd, user_cmd, sizeof(struct memory_container_cmd));
    
    //Setting calling thread's associated cid
    unsigned long long int cid = temp_cmd.cid;
    unsigned long long int oid = temp_cmd.oid;
    //Setting calling thread's associated pid
    int pid = current->pid;
    printk("\nInside unlock : CID -> %llu --- PID -> %d --- OID -> %llu", cid, pid, oid);

    return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
    mutex_lock(&my_mutex);
    struct memory_container_cmd temp_cmd;
    copy_from_user(&temp_cmd, user_cmd, sizeof(struct memory_container_cmd));
    
    //Setting calling thread's associated cid
    unsigned long long int cid = temp_cmd.cid;
    //Setting calling thread's associated pid
    int pid = current->pid;
    printk("\nInside Delete : CID -> %llu --- PID -> %d", cid, pid);
    struct container *temp_container;
    temp_container = container_head;
    while(temp_container)
    {
        if (temp_container->cid == cid)
        {    
            struct task *temp_task_head = temp_container->task_list;
            temp_task_head = deletetask(&temp_task_head, pid);
            printk("\n Task deleted : %d within Container : %llu", pid, cid); 
            temp_container->task_list = temp_task_head;
            if (!temp_task_head)
            {
                container_head = deletecontainer(&container_head, cid);
                printk("\n Container Deleted : %llu", cid);
                break;
            }
        }
        temp_container = temp_container->next;
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
    int pid = current->pid;
    struct container *temp_container;
    temp_container = container_head;
    while(temp_container)
    {
        if (temp_container->cid == cid)
        {
            struct object *temp_object_head = temp_container->object_list;
            if (temp_object_head)
            {
                kfree(object->address);
                object->address = NULL;
                temp_object_head = deleteobject(&temp_object_head,oid)
                temp_container -> object_list = temp_object_head;
            }
            if (!temp_object_head)
            {
                printk("\n No Object with oid: %d  exits in Container with cid: %llu", oid,cid);
                break;
            }
        }
        temp_container = temp_container->next;
    }
    display_list();
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
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

//Declaring a list to store container ids and a pointer to associated task ids
struct container {
    unsigned long long int cid;
    struct container *next;
    struct task *task_list;
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

void display_list(void)
{
    struct container *tc = container_head;
    while(tc)
    {
        struct task *tl = tc->task_list;
        while(tl)
        {
//            printk("\n CID : %llu ----  PID : %d State: %d", tc->cid, tl->currTask->pid, tl->currTask->state);
            tl=tl->next;
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
    return 0;
}


int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
    return 0;
}


int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{
    return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
    mutex_lock(&my_mutex);
    struct processor_container_cmd temp_cmd;
    copy_from_user(&temp_cmd, user_cmd, sizeof(struct processor_container_cmd));
    
    //Setting calling thread's associated cid
    unsigned long long int cid = temp_cmd.cid;
    //Setting calling thread's associated pid
    int pid = current->pid;

    struct container *temp_container;
    temp_container = container_head;
    while(temp_container)
    {
        if (temp_container->cid == cid)
        {    
            struct task *temp_task_head = temp_container->task_list;
            struct task *next_task;
            next_task = get_next_task(&temp_task_head, pid);
            if (next_task->currTask->pid != pid)
            {    
//                printk("\n PID: %d Waking next task PID: %d in CID: %llu before dying", pid, next_task->currTask->pid, cid);
                wake_up_process(next_task->currTask);
                mutex_unlock(&my_mutex);                
            }
            else{
//                printk("\n No next tasks for PID: %d in CID: %llu - killing self", pid, cid);
                mutex_unlock(&my_mutex);
            }
            temp_task_head = deletetask(&temp_task_head, pid);
//            printk("\n Task deleted : %d within Container : %llu", pid, cid); 
            temp_container->task_list = temp_task_head;
            if (!temp_task_head)
            {
                container_head = deletecontainer(&container_head, cid);
//                printk("\n Container Deleted : %llu", cid);
                break;
            }
        }
        temp_container = temp_container->next;
    }
//    printk("\nDeleting task : CID -> %llu --- PID -> %d", cid, pid);
//    display_list();
    return 0;
}


int memory_container_create(struct memory_container_cmd __user *user_cmd)
{
 
    //Mutex Lock
    mutex_lock(&my_mutex);

    struct processor_container_cmd temp_cmd;
    copy_from_user(&temp_cmd, user_cmd, sizeof(struct processor_container_cmd));
    
    //Setting calling thread's associated cid
    unsigned long long int cid = temp_cmd.cid;
    //Setting calling thread's associated pid
    int pid = current->pid;

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
        //Uncomment below code to see how tasks are getting allocated to containers
//        printk("\nCreating task : CID -> %llu --- PID -> %d", cid, pid);
//        display_list();
        mutex_unlock(&my_mutex);
    }
    else
    {
        //If Container was already present, then put all incoming tasks to sleep.
//        printk("\nCreating task : CID -> %llu --- PID -> %d", cid, pid);
//        printk("\nInitial Set to Sleep PID: %d in CID: %llu",pid,cid);
        set_current_state(TASK_INTERRUPTIBLE);
//        display_list();
        mutex_unlock(&my_mutex);
        schedule();
    }
    return 0;
}


int memory_container_free(struct memory_container_cmd __user *user_cmd)
{
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

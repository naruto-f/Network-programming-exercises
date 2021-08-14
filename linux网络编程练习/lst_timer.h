//
// Created by 123456 on 2021/8/14.
// 头文件介绍 : 实现一个升序的定时器链表数据结构类
//

#ifndef LINUX_LST_TIMER_H
#define LINUX_LST_TIMER_H

#include <time.h>
#include <netinet/in.h>

#define BUFF_SIZE 64

class util_timer;   /* 前向声明 */

/* 存放用户数据的结构体 */
struct client_data
{
    struct sockaddr_in addr;
    int sockfd;
    char buf[BUFF_SIZE];
    util_timer* timer;
};


/* 定时器类 */
class util_timer
{
public:
    util_timer() : prev(nullptr), next(nullptr) { }

public:
    time_t erpire;                           /* 超时时间，这里是绝对时间  */
    void ( *cb_func )( client_data* );       /* 任务回调函数(函数指针)   */
    client_data* user_data;
    util_timer* prev;
    util_timer* next;
};


/* 定时器链表，它是一个升序，双向链表，且带有头结点和尾结点 */
class sort_timer_lst
{
public:
    sort_timer_lst() : head(nullptr), tail(nullptr) { }
    ~sort_timer_lst()
    {
        util_timer* temp = head;
        while (temp)
        {
            /* 注意: 因为这里temp为循环条件，所以必须以temp所指向的删除对象 */
            head = temp->next;
            delete temp;
            temp = head;
        }
    }

    /* 将目标定时器添加到链表中 */
    void add_timer(util_timer* t)
    {
        if(!t)
        {
            return;
        }

        if((head == nullptr) && (tail == nullptr))
        {
            head = t;
            tail = t;
        }
        else if(t->erpire < head->erpire)
        {
            /* 插入链表头 */
            t->next = head;
            head->prev = t;
            head = t;
        }
        else if(t->erpire > tail->erpire)
        {
            /* 插入链表尾 */
            tail->next = t;
            t->prev = tail;
            t->next = nullptr;
            tail = t;
        }
        else
        {
            /* 使用重载的add_timer工具函数找到合适的位置插入 */
            add_timer(t, head);
        }
    }

    /* 当某个定时器任务发生变化时，调整对应的定时器在链表中的位置，这里只考虑定时器的超时时间延长的情况，即向链表尾部方向移动 */
    void adjust(util_timer* t)
    {
        if(!t)
        {
            return;
        }

        /* 如果目标定时器位于尾结点，或调整后的超时时间仍小于下一个定时器的超时时间，不用移动 */
        util_timer* next = t->next;
        if(tail == t || t->erpire < next->erpire)
        {
            return;
        }

        /* 如果目标定时器位于链表头，则要取下并寻找合适位置插入 */
        if(t == head)
        {
            head = head->next;
            head->prev = nullptr;
            t->next = nullptr;
            add_timer(t, head);
        }
        /* 如果目标定时器不是位于链表头，则取下后插入其原来所在位置之后的链表中 */
        else
        {
            t->prev->next = t->next;
            t->next->prev = t->prev;
            add_timer(t, next);
        }
    }

    void del_timer(util_timer* t)
    {
        if(!t)
        {
            return;
        }

        if(t == head && t == tail)
        {
            head = tail = nullptr;
            delete t;
            return;
        }

        if(t == head)
        {
            head = head->next;
            head->prev = nullptr;
            t->next = nullptr;
            delete t;
        }
        else if(t == tail)
        {
            tail = tail->prev;
            tail->next = nullptr;
            t->prev = nullptr;
            delete t;
        }
        else{
            t->prev->next = t->next;
            t->next->prev = t->prev;
            delete t;
        }
    }

    /* 定时器链表的核心函数: SIGALRM信号每次被触发就在其信号处理函数(如果是统一事件源，就在主程序中)调用一次，以处理链表上到期的任务 */
    void tick()
    {
        if(!head)
        {
            return;
        }

        util_timer* temp = head;
        /* 获取系统当前时间 */
        time_t cur = time(nullptr);

        /* 定时器的核心逻辑: 从头结点开始依次处理每个定时器，知道遇到一个尚未到期的定时器 */
        while(temp)
        {
            if (cur < temp->erpire)
            {
                break;
            }
            /* 执行任务函数,并将该定时器从链表中删除  */
            temp->cb_func(temp->user_data);
            head = temp->next;
            if(head)
            {
                head->prev = nullptr;
            }
            delete temp;
            temp = head;
        }
    }

private:
    /* 找到第一个大于t的超时时间的定时器，并插入其前 */
    void add_timer(util_timer* t, util_timer* begin_serach)
    {
        util_timer* prev = begin_serach;
        util_timer* temp = begin_serach->next;
        while(temp)
        {
            if(t->erpire < temp->erpire)
            {
                break;
            }
            prev = temp;
            temp = temp->next;
        }
        prev->next = t;
        t->prev = t;
        t->next = temp;
        temp->prev = t;
    }
    util_timer* head;      /* 链表首元素指针 */
    util_timer* tail;      /* 链表尾元素指针 */
};



#endif //LINUX_LST_TIMER_H

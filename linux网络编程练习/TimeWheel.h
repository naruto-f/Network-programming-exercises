//
// Created by 123456 on 2021/8/15.
// 库介绍 : 比升序定时器链表效率更高的简单时间轮数据结构
//

#ifndef LINUX_TIMEWHEEL_H
#define LINUX_TIMEWHEEL_H

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>

#define BUFF_SIZE 64

class timeWhell_timer;     /* 前向声明 */

/* 存放用户信息的结构体, 绑定socket和定时器 */
struct client_data
{
    struct sockaddr_in addr;
    int sockfd;
    char buf[BUFF_SIZE];
    timeWhell_timer* timer;
};

/* 时间轮使用的定时器 */
class timeWhell_timer
{
public:
    timeWhell_timer(int rt, int ts) : prev(nullptr), next(nullptr), rotation(rt), time_slot(ts) { }

public:
    int rotation;            /* 表示还需要转几圈才能执行定时任务 */
    int time_slot;           /* 表示该定时器在时间轮的哪个槽中 */
    void ( *cb_func ) ( client_data* );        /* 任务回调函数 */
    client_data* user_data;                    /* 用户数据 */
    timeWhell_timer* prev;
    timeWhell_timer* next;
};

/* 时间轮数据结构 */
class timeWhell
{
public:
    timeWhell() : cur_slot(0)
    {
        /* 初始化所有的定时器链表 */
        for(int i = 0; i < N; ++i)
        {
            tw_timer[i] = nullptr;
        }
    }

    ~timeWhell()
    {
        for(int i = 0; i < N; ++i)
        {
            timeWhell_timer* cur = tw_timer[i];
            while(cur)
            {
                tw_timer[i] = cur->next;
                delete cur;
                if(tw_timer[i])
                {
                    tw_timer[i]->prev = nullptr;
                }
                cur = tw_timer[i];
            }
        }
    }

    /* 根据给出的超时时间算出timeWhell_timer的rotation和time_slot,并插入到时间轮中合适的位置 */
    timeWhell_timer* add_timer(int timeout)
    {
        if(timeout < 0)
        {
            return nullptr;
        }

        int tick = 0;
        if(timeout < SI)
        {
            tick = 1;
        }
        else
        {
            tick = timeout / SI;
        }
        int rt = tick / N;
        int ts = (cur_slot + (tick % N)) % N;
        timeWhell_timer* tt = new timeWhell_timer(rt, ts);

        /* 如果要插入的槽为空 */
        if(!tw_timer[ts])
        {
            printf("add timer, rotation is %d, ts is %d cur_slot is %d\n", rt, ts, cur_slot);
            tw_timer[ts] = tt;
        }
        else{
            tw_timer[ts]->prev = tt;
            tt->next = tw_timer[ts];
            tw_timer[ts] = tt;
        }
        return tt;
    }

    void del_timer(timeWhell_timer* timer)
    {
        if(!timer)
        {
            return;
        }

        int ts = timer->time_slot;

        /* 如果该定时器是链表头 */
        if(timer == tw_timer[ts])
        {
            tw_timer[ts] = tw_timer[ts]->next;
            if(tw_timer[ts])
            {
                tw_timer[ts]->prev = nullptr;
            }
            delete timer;
        }
        /* 如果不是链表头 */
        else
        {
            timer->prev->next = timer->next;
            if(timer->next)
            {
                timer->next->prev = timer->prev;
            }
            delete timer;
        }
    }

    /* 心搏函数, 每隔si时间执行一次 */
    void tick()
    {
        timeWhell_timer* temp = tw_timer[cur_slot];
        printf("cur_slot is %d\n", cur_slot);
        while(temp)
        {
            printf("tick the timer once\n");
            if(temp->rotation > 0)
            {
                --temp->rotation;
                temp = temp->next;
            }
            /* 定时器到期，执行定时任务，并删除该定时器 */
            else{
                /* 执行定时器任务回调函数 */
                timeWhell_timer* next = temp->next;
                temp->cb_func(temp->user_data);
                del_timer(temp);
                temp = next;
            }
        }
        cur_slot = (++cur_slot) % N;
    }

private:
    static const int N = 60;       /* 槽的数量 */
    static const int SI = 2;       /* 两次执行tick函数的间隔 */
    timeWhell_timer* tw_timer[N];  /* 存放每个槽对应的定时器链表的首节点指针，这里的链表是无序的 */
    int cur_slot;                  /* 时间轮当前指向的槽的编号 */
};


#endif //LINUX_TIMEWHEEL_H

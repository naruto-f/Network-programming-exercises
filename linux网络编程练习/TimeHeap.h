//
// Created by 123456 on 2021/8/15.
// 库文件介绍 : 使用用数组表示的小顶堆实现的高性能定时器管理数据结构时间堆
//

#ifndef LINUX_TIMEHEAP_H
#define LINUX_TIMEHEAP_H

#include <iostream>
#include <time.h>
#include <netinet/in.h>

#define BUFF_SIZE 64

class heap_timer;      /* 前向声明 */

/* 存放客户数据的数据结构 */
struct client_data
{
    struct sockaddr_in addr;
    int sockfd;
    char buf[BUFF_SIZE];
    heap_timer* timer;
};

/* 时间堆使用的定时器 */
class heap_timer
{
public:
    heap_timer(int timeout) : erpire(time(nullptr) + timeout) { }

public:
    time_t erpire;                               /* 定时器超时时间, 是绝对时间 */
    void ( *cb_func ) ( struct client_data* );   /* 定时器任务回调函数 */
    struct client_data* user_data;
};

/* 时间堆*/
class time_heap
{
public:
    /* 只传入数组容量的构造函数 */
    time_heap(int cap) noexcept : capacity(cap), cur_size(0)
    {
        array = new heap_timer*[cap];
        if(array)
        {
            for(int i = 0; i < capacity; ++i)
            {
                array[i] = nullptr;
            }
        }
    }

    /* 传入已存在的heap_timer*数组的构造函数 */
    time_heap(heap_timer** old_array, int size, int cap) noexcept
    : capacity(cap), cur_size(size)
    {
        while(capacity < size)
        {
            resize();
        }

        array = new heap_timer*[cap];
        if(array)
        {
            for(int i = 0; i < capacity; ++i)
            {
                array[i] = nullptr;
            }

            if(size > 0)
            {
                /* 初始化堆数组 */
                for(int i = 0; i < cur_size; ++i)
                {
                    array[i] = old_array[i];
                }
                for(int i = (cur_size - 1) / 2; i >= 0; --i)
                {
                    /* 从最后一个非叶子结点向根节点方向开始依次执行下虑操作 */
                    percolate_down(i);
                }
            }
        }
    }

    ~time_heap()
    {
        for(int i = 0; i < cur_size; ++i)
        {
            delete array[i];
        }
        delete [] array;
    }

public:
    void add_timer(heap_timer* timer) noexcept
    {
        if(!timer)
        {
            return;
        }

        if(++cur_size > capacity)
        {
            resize();
        }

        int pos = cur_size - 1;
        int parent = 0;
        /* 堆上虑操作 */
        for(; pos > 0; pos = parent)
        {
            parent = (pos - 1) / 2;
            if(timer->erpire < array[parent]->erpire)
            {
                array[pos] = array[parent];
            }
            else{
                break;
            }
        }
        array[pos] = timer;
    }

    void del_timer(heap_timer* timer)
    {
        /* 延迟删除，但是可能会使数组膨胀 */
        if(!timer)
        {
            return;
        }
        timer->cb_func = nullptr;
    }

    /* 获取堆顶 */
    heap_timer* top() const
    {
        if(empty())
        {
            return nullptr;
        }
        else
        {
            return array[0];
        }
    }

    /* 删除堆顶并整理堆使其继续满足小顶堆 */
    void pop()
    {
        if(empty())
        {
            return;
        }
        else{
            delete array[0];
            array[0] = array[--cur_size];
            /* 对新的堆顶元素执行下虑操作 */
            percolate_down(0);
        }
    }

    /* 堆是否为空 */
    bool empty() const
    {
        return cur_size == 0;
    }

    /* 心搏函数 */
    void tick()
    {
        if(empty())
        {
            return;
        }
        heap_timer* temp = array[0];
        time_t cur = time(nullptr);
        /* 循环检测哪些定时器超时，并执行其任务回调函数 */
        while(!empty())
        {
            if (!temp)
            {
                break;
            }

            if(temp->erpire > cur)
            {
                break;
            }

            if(temp->cb_func)
            {
                temp->cb_func(temp->user_data);
            }
            pop();
            temp = array[0];
        }
    }

private:
    /* 堆的下虑操作 : 从pos位置开始，与两个子女结点的最小值交换位置，循环这样做直到找到一个合适的位置 */
    void percolate_down(int pos)
    {
        if(pos < 0)
        {
            return;
        }
        heap_timer* temp = array[pos];
        int child = 0;
        for(; (pos * 2 + 1) <= cur_size - 1; pos = child)
        {
            child = 2 * pos + 1;
            if(child < cur_size - 1 && array[child + 1]->erpire < array[child]->erpire)
            {
                ++child;
            }

            if(array[pos]->erpire > array[child]->erpire)
            {
                array[pos] = array[child];
            }
            else{
                /* 以temp为根节点的子树已经是一个小顶堆 */
                break;
            }
        }
        array[pos] = temp;
    }

    /* 将数组容量capcity翻倍 */
    void resize() noexcept
    {
        heap_timer** new_array = new heap_timer*[capacity * 2];
        if(new_array)
        {
            for(int i = 0; i < 2 * capacity; ++i)
            {
                new_array[i] = nullptr;
            }
            for(int i = 0; i < cur_size; ++i)
            {
                new_array[i] = array[i];
            }

            delete [] array;
            capacity = 2 * capacity;
            array = new_array;
        }
    }

    heap_timer** array;    /* 即存放定时器指针的数组，这里使用数组来模拟小顶堆 */
    int capacity;          /* 当前数组最大容量 */
    int cur_size;          /* 当前数组已存放元素数 */
};


#endif //LINUX_TIMEHEAP_H

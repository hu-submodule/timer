/**
 * @file      hs_timer.c
 * @brief     定时器模块源文件
 * @author    huenrong (sgyhy1028@outlook.com)
 * @date      2026-02-01 14:23:19
 *
 * @copyright Copyright (c) 2026 huenrong
 *
 */

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "hs_timer.h"

// 定时器状态
typedef enum hs_timer_status
{
    E_HS_TIMER_STATUS_CREATED = 1,     // 已创建
    E_HS_TIMER_STATUS_RUNNING,         // 运行中
    E_HS_TIMER_STATUS_PAUSED,          // 已暂停
    E_HS_TIMER_STATUS_REQUEST_DESTROY, // 请求销毁
} hs_timer_status_e;

// 定时器对象
struct _hs_timer
{
    timer_t timer_id;         // 定时器ID
    pthread_mutex_t mutex;    // 互斥锁
    hs_timer_status_e status; // 定时器状态
    hs_timer_cb timer_cb;     // 定时器回调函数
    uint32_t repeat_count;    // 定时器重复次数 (1: 执行一次; UINT32_MAX: 无限循环)
    uint32_t timeout_ms;      // 定时器超时时间 (单位: ms)
    const void *user_data;    // 用户数据
};

/**
 * @brief 判断定时器是否可以设置参数
 *
 * @param[in] hs_timer: 定时器对象
 *
 * @return true : 可以
 * @return false: 不可以
 */
static bool hs_timer_can_set_params(const hs_timer_t *hs_timer)
{
    if (hs_timer == NULL)
    {
        return false;
    }

    if ((hs_timer->status == E_HS_TIMER_STATUS_CREATED) || (hs_timer->status == E_HS_TIMER_STATUS_RUNNING) ||
        (hs_timer->status == E_HS_TIMER_STATUS_PAUSED))
    {
        return true;
    }

    return false;
}

/**
 * @brief 将毫秒转换为 itimerspec 结构体
 *
 * @note 该函数只适用于单次定时器
 *
 * @param[in]  ms        : 毫秒数
 * @param[out] timer_spec: 转换后的 itimerspec 结构体
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
static int hs_timer_ms_to_timespec_one_shot(const uint32_t ms, struct itimerspec *timer_spec)
{
    if (timer_spec == NULL)
    {
        return -1;
    }

    // 因为使用 one-shot 模式，这里不设置 it_interval。在回调函数中，根据 repeat_count 决定是否再次启动
    timer_spec->it_interval.tv_sec = 0;
    timer_spec->it_interval.tv_nsec = 0;

    timer_spec->it_value.tv_sec = ms / 1000;
    timer_spec->it_value.tv_nsec = (ms % 1000) * 1000000;

    return 0;
}

/**
 * @brief 定时器回调线程
 *
 * @param[in] sigev_value: 用户自定义参数，在调用timer_create()时传入
 */
static void hs_timer_thread(union sigval sigev_value)
{
    hs_timer_t *hs_timer = (hs_timer_t *)sigev_value.sival_ptr;
    if (hs_timer == NULL)
    {
        return;
    }

    pthread_mutex_lock(&hs_timer->mutex);

    // 已请求销毁，则销毁定时器
    if (hs_timer->status == E_HS_TIMER_STATUS_REQUEST_DESTROY)
    {
        timer_delete(hs_timer->timer_id);
        pthread_mutex_unlock(&hs_timer->mutex);
        pthread_mutex_destroy(&hs_timer->mutex);
        free(hs_timer);

        return;
    }

    // 先把 repeat_count 减一，防止回调函数会根据该值判断是否是最后一次运行或者是否需要销毁定时器
    if ((hs_timer->repeat_count > 0) && (hs_timer->repeat_count < UINT32_MAX))
    {
        hs_timer->repeat_count--;

        if (hs_timer->repeat_count == 0)
        {
            hs_timer->status = E_HS_TIMER_STATUS_REQUEST_DESTROY;
        }
    }
    pthread_mutex_unlock(&hs_timer->mutex);

    // 用户回调不加锁，防止在回调函数设置参数等造成死锁
    if (hs_timer->timer_cb != NULL)
    {
        hs_timer->timer_cb(hs_timer);
    }

    // 在回调函数中请求了销毁定时器或重复次数归0，立即销毁，万一超时时间很长，销毁速度太慢了
    pthread_mutex_lock(&hs_timer->mutex);
    if ((hs_timer->status == E_HS_TIMER_STATUS_REQUEST_DESTROY) || (hs_timer->repeat_count == 0))
    {
        timer_delete(hs_timer->timer_id);
        pthread_mutex_unlock(&hs_timer->mutex);
        pthread_mutex_destroy(&hs_timer->mutex);
        free(hs_timer);

        return;
    }

    // 没有请求销毁定时器，则重新启动定时器
    if ((hs_timer->status == E_HS_TIMER_STATUS_RUNNING) && (hs_timer->repeat_count != 0))
    {
        struct itimerspec timer_spec = {0};
        if (hs_timer_ms_to_timespec_one_shot(hs_timer->timeout_ms, &timer_spec) != 0)
        {
            pthread_mutex_unlock(&hs_timer->mutex);

            return;
        }

        timer_settime(hs_timer->timer_id, 0, &timer_spec, NULL);
    }

    pthread_mutex_unlock(&hs_timer->mutex);
}

hs_timer_t *hs_timer_create(void)
{
    hs_timer_t *hs_timer = (hs_timer_t *)malloc(sizeof(hs_timer_t));
    if (hs_timer == NULL)
    {
        return NULL;
    }

    hs_timer->status = E_HS_TIMER_STATUS_CREATED;
    hs_timer->timer_cb = NULL;
    hs_timer->repeat_count = -1;
    hs_timer->timeout_ms = 0;
    hs_timer->user_data = NULL;
    pthread_mutex_init(&hs_timer->mutex, NULL);

    return hs_timer;
}

int hs_timer_init(hs_timer_t *hs_timer, const hs_timer_cb timer_cb, const uint32_t repeat_count,
                  const uint32_t timeout_ms, const void *user_data)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);

    // 定时器已初始化，关闭后再初始化
    if (hs_timer->status != E_HS_TIMER_STATUS_CREATED)
    {
        timer_delete(hs_timer->timer_id);
    }

    struct sigevent sig_event = {0};
    memset(&sig_event, 0, sizeof(struct sigevent));
    sig_event.sigev_notify = SIGEV_THREAD;
    sig_event.sigev_notify_function = hs_timer_thread;
    sig_event.sigev_value.sival_ptr = hs_timer;
    // CLOCK_MONOTONIC: 获取的时间为系统重启到现在的时间, 更改系统时间对其没有影响
    if (timer_create(CLOCK_MONOTONIC, &sig_event, &hs_timer->timer_id) == -1)
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -3;
    }

    struct itimerspec timer_spec = {0};
    if (hs_timer_ms_to_timespec_one_shot(timeout_ms, &timer_spec) != 0)
    {
        timer_delete(hs_timer->timer_id);
        pthread_mutex_unlock(&hs_timer->mutex);

        return -4;
    }

    if (timer_settime(hs_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        timer_delete(hs_timer->timer_id);
        pthread_mutex_unlock(&hs_timer->mutex);

        return -5;
    }

    hs_timer->status = E_HS_TIMER_STATUS_RUNNING;
    hs_timer->timer_cb = timer_cb;
    hs_timer->repeat_count = repeat_count;
    hs_timer->timeout_ms = timeout_ms;
    hs_timer->user_data = user_data;

    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

int hs_timer_destroy(hs_timer_t *hs_timer)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);

    switch (hs_timer->status)
    {
    // 未初始化，立即销毁
    case E_HS_TIMER_STATUS_CREATED:
    {
        pthread_mutex_unlock(&hs_timer->mutex);
        pthread_mutex_destroy(&hs_timer->mutex);
        free(hs_timer);

        return 0;
    }

    // 运行中，异步销毁
    case E_HS_TIMER_STATUS_RUNNING:
    {
        hs_timer->status = E_HS_TIMER_STATUS_REQUEST_DESTROY;
        pthread_mutex_unlock(&hs_timer->mutex);

        return 0;
    }

    // 暂停中，立即销毁
    case E_HS_TIMER_STATUS_PAUSED:
    {
        timer_delete(hs_timer->timer_id);
        pthread_mutex_unlock(&hs_timer->mutex);
        pthread_mutex_destroy(&hs_timer->mutex);
        free(hs_timer);

        return 0;
    }

    // 销毁中，忽略
    case E_HS_TIMER_STATUS_REQUEST_DESTROY:
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return 0;
    }

    default:
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -2;
    }
    }
}

int hs_timer_set_cb(hs_timer_t *hs_timer, const hs_timer_cb timer_cb)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    if (!hs_timer_can_set_params(hs_timer))
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -2;
    }

    hs_timer->timer_cb = timer_cb;
    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

int hs_timer_set_repeat_count(hs_timer_t *hs_timer, const uint32_t repeat_count)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    if (!hs_timer_can_set_params(hs_timer))
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -2;
    }

    hs_timer->repeat_count = repeat_count;
    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

int hs_timer_set_timeout(hs_timer_t *hs_timer, const uint32_t timeout_ms)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    if (!hs_timer_can_set_params(hs_timer))
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -2;
    }

    struct itimerspec timer_spec = {0};
    if (hs_timer_ms_to_timespec_one_shot(timeout_ms, &timer_spec) != 0)
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -3;
    }

    if (timer_settime(hs_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -4;
    }

    hs_timer->timeout_ms = timeout_ms;
    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

int hs_timer_set_user_data(hs_timer_t *hs_timer, const void *user_data)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    if (!hs_timer_can_set_params(hs_timer))
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -2;
    }

    hs_timer->user_data = user_data;
    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

int hs_timer_get_repeat_count(hs_timer_t *hs_timer, uint32_t *repeat_count)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    *repeat_count = hs_timer->repeat_count;
    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

int hs_timer_get_timeout(hs_timer_t *hs_timer, uint32_t *timeout_ms)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    *timeout_ms = hs_timer->timeout_ms;
    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

const void *hs_timer_get_user_data(hs_timer_t *hs_timer)
{
    if (hs_timer == NULL)
    {
        return NULL;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    const void *user_data = hs_timer->user_data;
    pthread_mutex_unlock(&hs_timer->mutex);

    return user_data;
}

int hs_timer_ready(hs_timer_t *hs_timer)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    if ((hs_timer->status != E_HS_TIMER_STATUS_RUNNING) && (hs_timer->status != E_HS_TIMER_STATUS_PAUSED))
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -2;
    }

    // 因为使用 one-shot 模式，这里不设置 it_interval。在回调函数中，根据 repeat_count 决定是否再次启动
    struct itimerspec timer_spec = {0};
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_interval.tv_nsec = 0;
    timer_spec.it_value.tv_sec = 0;
    timer_spec.it_value.tv_nsec = 1;
    if (timer_settime(hs_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -3;
    }

    hs_timer->status = E_HS_TIMER_STATUS_RUNNING;
    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

int hs_timer_pause(hs_timer_t *hs_timer)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    if ((hs_timer->status != E_HS_TIMER_STATUS_RUNNING) && (hs_timer->status != E_HS_TIMER_STATUS_PAUSED))
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -2;
    }

    struct itimerspec timer_spec = {0};
    if (timer_settime(hs_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -3;
    }

    hs_timer->status = E_HS_TIMER_STATUS_PAUSED;
    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

int hs_timer_resume(hs_timer_t *hs_timer)
{
    if (hs_timer == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    if (hs_timer->status != E_HS_TIMER_STATUS_PAUSED)
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -2;
    }

    struct itimerspec timer_spec = {0};
    if (hs_timer_ms_to_timespec_one_shot(hs_timer->timeout_ms, &timer_spec) != 0)
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -3;
    }

    if (timer_settime(hs_timer->timer_id, 0, &timer_spec, NULL) == -1)
    {
        pthread_mutex_unlock(&hs_timer->mutex);

        return -4;
    }

    hs_timer->status = E_HS_TIMER_STATUS_RUNNING;
    pthread_mutex_unlock(&hs_timer->mutex);

    return 0;
}

bool hs_timer_is_paused(hs_timer_t *hs_timer)
{
    if (hs_timer == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&hs_timer->mutex);
    bool is_paused = (hs_timer->status == E_HS_TIMER_STATUS_PAUSED);
    pthread_mutex_unlock(&hs_timer->mutex);

    return is_paused;
}

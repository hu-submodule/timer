/**
 * @file      hs_timer.h
 * @brief     定时器模块头文件
 * @author    huenrong (sgyhy1028@outlook.com)
 * @date      2026-02-01 14:23:26
 *
 * @copyright Copyright (c) 2026 huenrong
 *
 */

#ifndef __HS_TIMER_H
#define __HS_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HS_TIMER_REPEAT_ONCE    (1U)         // 定时器执行一次
#define HS_TIMER_REPEAT_FOREVER (UINT32_MAX) // 定时器无限循环

// 定时器对象
typedef struct _hs_timer hs_timer_t;

/**
 * @brief 定时器回调函数
 *
 * @param[in,out] hs_timer: 定时器对象
 */
typedef void (*hs_timer_cb)(hs_timer_t *hs_timer);

/**
 * @brief 创建定时器对象
 *
 * @return 成功: 定时器对象
 * @return 失败: NULL
 */
hs_timer_t *hs_timer_create(void);

/**
 * @brief 初始化定时器对象
 *
 * @note 1. 初始化后，会直接启动定时器
 *       2. 该函数支持重复调用，重复调用时会重新关闭并重新启动定时器
 *       3. 调用该函数前必须确保没有其它线程正在使用该定时器对象，否则可能导致未定义行为
 *
 * @param[in,out] hs_timer    : 定时器对象
 * @param[in]     timer_cb    : 定时器回调函数
 * @param[in]     repeat_count: 定时器重复次数
 *                              HS_TIMER_REPEAT_ONCE: 执行一次
 *                              HS_TIMER_REPEAT_FOREVER: 无限循环
 * @param[in]     timeout     : 定时器超时时间 (单位: ms)
 * @param[in]     user_data   : 用户数据
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_init(hs_timer_t *hs_timer, const hs_timer_cb timer_cb, const uint32_t repeat_count, const uint32_t timeout,
                  const void *user_data);

/**
 * @brief 销毁定时器对象
 *
 * @note 1. 未初始化的定时器会立即销毁
 *       2. 已初始化的定时器不会立即销毁，内部在合适的时机进行销毁
 *       3. 销毁后，定时器对象将不再可用
 *
 * @param[in,out] hs_timer: 定时器对象
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_destroy(hs_timer_t *hs_timer);

/**
 * @brief 设置定时器回调函数
 *
 * @param[in,out] hs_timer: 定时器对象
 * @param[in]     timer_cb: 定时器回调函数
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_set_cb(hs_timer_t *hs_timer, const hs_timer_cb timer_cb);

/**
 * @brief 设置定时器重复次数
 *
 * @param[in,out] hs_timer    : 定时器对象
 * @param[in]     repeat_count: 定时器重复次数
 *                              HS_TIMER_REPEAT_ONCE: 执行一次
 *                              HS_TIMER_REPEAT_FOREVER: 无限循环
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_set_repeat_count(hs_timer_t *hs_timer, const uint32_t repeat_count);

/**
 * @brief 设置定时器超时时间
 *
 * @param[in,out] hs_timer  : 定时器对象
 * @param[in]     timeout_ms: 定时器超时时间 (单位: ms)
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_set_timeout(hs_timer_t *hs_timer, const uint32_t timeout_ms);

/**
 * @brief 设置定时器用户数据
 *
 * @param[in,out] hs_timer : 定时器对象
 * @param[in]     user_data: 用户数据
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_set_user_data(hs_timer_t *hs_timer, const void *user_data);

/**
 * @brief 获取定时器重复次数
 *
 * @param[in,out] hs_timer    : 定时器对象
 * @param[out]    repeat_count: 定时器重复次数
 *                              HS_TIMER_REPEAT_ONCE: 执行一次
 *                              HS_TIMER_REPEAT_FOREVER: 无限循环
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_get_repeat_count(hs_timer_t *hs_timer, uint32_t *repeat_count);

/**
 * @brief 获取定时器超时时间
 *
 * @param[in,out] hs_timer  : 定时器对象
 * @param[out]    timeout_ms: 定时器超时时间 (单位: ms)
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_get_timeout(hs_timer_t *hs_timer, uint32_t *timeout_ms);

/**
 * @brief 获取定时器用户数据
 *
 * @param[in,out] hs_timer: 定时器对象
 *
 * @return 用户数据
 */
const void *hs_timer_get_user_data(hs_timer_t *hs_timer);

/**
 * @brief 设置定时器就绪
 *
 * @note 本次立即执行，后续根据定时器超时时间执行
 *
 * @param[in,out] hs_timer: 定时器对象
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_ready(hs_timer_t *hs_timer);

/**
 * @brief 暂停定时器
 *
 * @param[in,out] hs_timer: 定时器对象
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_pause(hs_timer_t *hs_timer);

/**
 * @brief 恢复定时器
 *
 * @param[in,out] hs_timer: 定时器对象
 *
 * @return 0 : 成功
 * @return <0: 失败
 */
int hs_timer_resume(hs_timer_t *hs_timer);

/**
 * @brief 定时器是否被暂停
 *
 * @param[in] hs_timer: 定时器对象
 *
 * @return true : 已被暂停
 * @return false: 定时器未初始化，发生错误，未被暂停
 */
bool hs_timer_is_paused(hs_timer_t *hs_timer);

#ifdef __cplusplus
}
#endif

#endif // __HS_TIMER_H

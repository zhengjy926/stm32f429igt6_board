/**
  ******************************************************************************
  * @file      : gpio_stm32.h
  * @author    : ZJY
  * @version   : V1.1
  * @date      : 2026-11-23
  * @brief     : STM32F4 GPIO 端口设备声明
  * @attention : 设备由 gpio_stm32.c 用 DEVICE_DEFINE 注册，名字固定为
  *              gpioa ~ gpiok；上层只通过 gpio.h 的接口访问，
  *              config/api 是驱动私有类型，不要直接解引用。
  ******************************************************************************
  * @history   :
  *       V1.0 : 初版
  *       V1.1 : 1. 新增 GPIO_STM32_PINS_PER_PORT：端口宽度属于芯片属性，
  *                从 gpio.h 下沉到驱动侧，写死 16 只影响 STM32 驱动
  ******************************************************************************
  */
#ifndef DRIVERS_GPIO_STM32_H_
#define DRIVERS_GPIO_STM32_H_

#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported constants --------------------------------------------------------*/

/**
 * @brief STM32F4 每个 GPIO 端口固定 16 脚
 *
 * @note 这是 STM32 的属性，不是通用 GPIO 接口的约定：gpio.h 只给出
 *       GPIO_PORT_MAX_PINS（掩码位宽上限），实际端口宽度由驱动自己声明。
 *       换到端口宽度不同的 MCU 时，改这里并确认 GPIO_PORT_MAX_PINS 够宽。
 */
#define GPIO_STM32_PINS_PER_PORT 16U

/* Exported variables --------------------------------------------------------*/
DEVICE_DECLARE(gpioa);
DEVICE_DECLARE(gpiob);
DEVICE_DECLARE(gpioc);
DEVICE_DECLARE(gpiod);
DEVICE_DECLARE(gpioe);
DEVICE_DECLARE(gpiof);
DEVICE_DECLARE(gpiog);
DEVICE_DECLARE(gpioh);
DEVICE_DECLARE(gpioi);
DEVICE_DECLARE(gpioj);
DEVICE_DECLARE(gpiok);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DRIVERS_GPIO_STM32_H_ */

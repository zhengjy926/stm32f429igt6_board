/**
  ******************************************************************************
  * @file      : gpio_stm32.h
  * @author    : ZJY
  * @version   : V1.0
  * @date      : 2026-11-20
  * @brief     : STM32F4 GPIO 端口设备声明
  * @attention : 设备由 gpio_stm32.c 用 DEVICE_DEFINE 注册，名字固定为
  *              gpioa ~ gpiok；上层只通过 gpio.h 的接口访问，
  *              config/api 是驱动私有类型，不要直接解引用。
  ******************************************************************************
  */
#ifndef DRIVERS_GPIO_STM32_H_
#define DRIVERS_GPIO_STM32_H_

#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

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

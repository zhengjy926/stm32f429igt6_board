/**
  ******************************************************************************
  * @file      : gpio.h
  * @author    : ZJY
  * @version   : V1.1
  * @date      : 2026-11-20
  * @brief     : GPIO 通用设备驱动接口，与具体 MCU 无关
  * @attention : 返回值约定：成功返回 0，失败返回负 errno（见 libs/err_def.h）。
  *              调用约定：
  *              - 调用前须确认 device_is_ready(dev) 为真，本层不检查初始化状态；
  *              - 本层只做 dev / 出参判空（返回 -EINVAL），不吞驱动错误码；
  *              - 驱动侧 vtable 所有函数指针必须非 NULL；暂不支持的操作用
  *                stub 返回 -ENOTSUP，不要留空指针。
  *              本层只负责通用 GPIO（输入/输出），复用功能（AF）与模拟模式
  *              由外设驱动自行配置 MODER/AFR/EXTICR，不在此接口内。
  ******************************************************************************
  * @history   :
  *       V1.0 : 初版
  *       V1.1 : 1. <cstdint> 改为 <stdint.h>，C/C++ 均可编译
  *              2. 删除重复定义的 gpio_port_toggle()
  *              3. 头文件保护符去掉保留前缀，改为 DRIVERS_GPIO_H_
  *              4. 补齐 pin_configure() 的 flags 位定义（方向/输出类型/上下拉/初始电平）
  *              5. 中断改为 "配置 + pending/ack" 闭环，避免 EXTI 挂起位不清导致中断风暴
  *              6. 统一命名：gpio_drive_api -> gpio_driver_api；port_xxx 的 value 明确为 mask
  ******************************************************************************
  */
#ifndef DRIVERS_GPIO_H_
#define DRIVERS_GPIO_H_

#include <stdint.h>

#include "device.h"
#include "err_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported constants --------------------------------------------------------*/

/** @brief 单个 GPIO 端口的引脚数（STM32F4 每个端口 16 脚） */
#define GPIO_PINS_PER_PORT  16U

/** @brief 端口全引脚掩码 */
#define GPIO_PORT_PIN_MASK_ALL  ((gpio_mask_t)0xFFFFU)

/** @brief 引脚号，有效范围 0 ~ GPIO_PINS_PER_PORT-1 */
typedef uint8_t gpio_pin_t;

/** @brief 端口引脚掩码，bit n 对应 pin n；未使用的位必须为 0 */
typedef uint16_t gpio_mask_t;

/** @brief gpio_pin_configure() 的配置字，由下方各标记按位或组成 */
typedef uint32_t gpio_flags_t;

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 中断触发方式
 *
 * @note 电平触发与"上升沿+下降沿"是互斥的，所以合并成一个枚举，
 *       不再拆 int_type / int_trig 两个字段（原设计里 LEVEL + BOTH 无意义）。
 *       电平触发需要 MCU 的 EXTI 支持；STM32F4 只有边沿触发，
 *       驱动对 LEVEL_HIGH / LEVEL_LOW 返回 -ENOTSUP。
 */
typedef enum {
    GPIO_INT_NONE         = 0,  /**< 关闭该引脚中断 */
    GPIO_INT_EDGE_RISING  = 1,  /**< 上升沿 */
    GPIO_INT_EDGE_FALLING = 2,  /**< 下降沿 */
    GPIO_INT_EDGE_BOTH    = 3,  /**< 双边沿 */
    GPIO_INT_LEVEL_HIGH   = 4,  /**< 高电平 */
    GPIO_INT_LEVEL_LOW    = 5,  /**< 低电平 */
} gpio_int_mode_t;

/* Exported macros -----------------------------------------------------------*/

/*
 * flags 位域布局（未定义位必须为 0，否则 gpio_pin_configure() 返回 -EINVAL）：
 *
 *   bit  字段     取值
 *   [0]   dir     0=输入 1=输出                     （对应 MODER）
 *   [1]   otype   0=推挽 1=开漏                     （对应 OTYPER，dir=输出时有效）
 *   [3:2] pull    0=悬空 1=上拉 2=下拉              （对应 PUPDR）
 *   [4]   init    0=初始低 1=初始高                 （dir=输出时有效）
 *   [31:5] 保留
 *
 * 用法示例：
 *   PA13 推挽输出、上电为高：
 *       gpio_pin_configure(dev, 13, GPIO_DIR_OUTPUT | GPIO_PUSH_PULL | GPIO_INIT_HIGH);
 *   PA0  按键上拉输入：
 *       gpio_pin_configure(dev, 0,  GPIO_DIR_INPUT  | GPIO_PULL_UP);
 *   PA5  开漏输出、初始高：
 *       gpio_pin_configure(dev, 5,  GPIO_DIR_OUTPUT | GPIO_OPEN_DRAIN | GPIO_INIT_HIGH);
 */

/* 方向 dir：bit[0] */
#define GPIO_DIR_POS     0U
#define GPIO_DIR_MASK    ((gpio_flags_t)0x1U << GPIO_DIR_POS)
#define GPIO_DIR_INPUT   ((gpio_flags_t)0x0U << GPIO_DIR_POS)  /**< 输入（复位默认值） */
#define GPIO_DIR_OUTPUT  ((gpio_flags_t)0x1U << GPIO_DIR_POS)  /**< 输出 */

/* 输出类型 otype：bit[1]，仅 dir = OUTPUT 时有效 */
#define GPIO_OTYPE_POS   1U
#define GPIO_OTYPE_MASK  ((gpio_flags_t)0x1U << GPIO_OTYPE_POS)
#define GPIO_PUSH_PULL   ((gpio_flags_t)0x0U << GPIO_OTYPE_POS) /**< 推挽 */
#define GPIO_OPEN_DRAIN  ((gpio_flags_t)0x1U << GPIO_OTYPE_POS) /**< 开漏 */

/* 上下拉 pull：bits[3:2] */
#define GPIO_PULL_POS    2U
#define GPIO_PULL_MASK   ((gpio_flags_t)0x3U << GPIO_PULL_POS)
#define GPIO_PULL_NONE   ((gpio_flags_t)0x0U << GPIO_PULL_POS)  /**< 悬空 */
#define GPIO_PULL_UP     ((gpio_flags_t)0x1U << GPIO_PULL_POS)  /**< 上拉 */
#define GPIO_PULL_DOWN   ((gpio_flags_t)0x2U << GPIO_PULL_POS)  /**< 下拉 */

/* 初始输出电平 init：bit[4]，仅 dir = OUTPUT 时有效 */
#define GPIO_INIT_POS    4U
#define GPIO_INIT_MASK   ((gpio_flags_t)0x1U << GPIO_INIT_POS)
#define GPIO_INIT_LOW    ((gpio_flags_t)0x0U << GPIO_INIT_POS)  /**< 初始输出低 */
#define GPIO_INIT_HIGH   ((gpio_flags_t)0x1U << GPIO_INIT_POS)  /**< 初始输出高 */

/** @brief flags 中已定义位的并集，驱动用它判非法位 */
#define GPIO_FLAGS_VALID (GPIO_DIR_MASK | GPIO_OTYPE_MASK | \
                          GPIO_PULL_MASK | GPIO_INIT_MASK)

/**
 * @brief 驱动 vtable
 *
 * @note 掩码类参数一律叫 mask，电平/出参各自叫 value。
 *       set/clear 走 BSRR，单次写、天然原子；toggle 是 ODR 读改写，非原子，
 *       多上下文并发翻同一个口需上层自行加锁。
 */
struct gpio_driver_api {
    int (*pin_configure)(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags);
    int (*pin_set)(const struct device *dev, gpio_pin_t pin, uint32_t value);
    int (*pin_get)(const struct device *dev, gpio_pin_t pin, uint32_t *value);
    int (*pin_toggle)(const struct device *dev, gpio_pin_t pin);
    int (*pin_interrupt_configure)(const struct device *dev, gpio_pin_t pin,
                                   gpio_int_mode_t mode);
    int (*pin_irq_ack)(const struct device *dev, gpio_pin_t pin);
    int (*port_get)(const struct device *dev, gpio_mask_t *value);
    int (*port_set_bits)(const struct device *dev, gpio_mask_t mask);
    int (*port_clear_bits)(const struct device *dev, gpio_mask_t mask);
    int (*port_toggle_bits)(const struct device *dev, gpio_mask_t mask);
    int (*port_irq_status)(const struct device *dev, gpio_mask_t *pending);
};

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  配置引脚方向、输出类型、上下拉和初始电平
 * @param  dev   GPIO 设备指针
 * @param  pin   引脚号 0 ~ GPIO_PINS_PER_PORT-1
 * @param  flags 位域见上方布局
 * @retval 0       成功
 * @retval -EINVAL dev 为空、pin 越界或 flags 含未定义位
 */
static inline int gpio_pin_configure(const struct device *dev, gpio_pin_t pin,
                                     gpio_flags_t flags)
{
    if ((dev == NULL) || (dev->api == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->pin_configure(dev, pin, flags);
}

/**
 * @brief  写引脚电平
 * @param  value 0 输出低，非 0 输出高
 * @retval 0      成功
 * @retval -EINVAL dev 为空或 pin 越界
 */
static inline int gpio_pin_set(const struct device *dev, gpio_pin_t pin, uint32_t value)
{
    if ((dev == NULL) || (dev->api == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->pin_set(dev, pin, value);
}

/**
 * @brief  读引脚电平
 * @param  value 出参：0 低电平，1 高电平
 * @retval 0      成功
 * @retval -EINVAL dev/value 为空或 pin 越界
 * @note   读的是 IDR 物理电平，不做 ACTIVE_LOW 反相。若后续需要"按下=1"的
 *         逻辑语义，应在 flags 里加 ACTIVE_LOW 并由驱动保存每脚极性后处理，
 *         不要在本层静默反相。
 */
static inline int gpio_pin_get(const struct device *dev, gpio_pin_t pin, uint32_t *value)
{
    if ((dev == NULL) || (dev->api == NULL) || (value == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->pin_get(dev, pin, value);
}

/**
 * @brief  翻转引脚电平
 * @retval 0      成功
 * @retval -EINVAL dev 为空或 pin 越界
 * @note   读改写 ODR，非原子。
 */
static inline int gpio_pin_toggle(const struct device *dev, gpio_pin_t pin)
{
    if ((dev == NULL) || (dev->api == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->pin_toggle(dev, pin);
}

/**
 * @brief  配置引脚中断触发方式
 * @param  mode 触发方式；GPIO_INT_NONE 表示关闭
 * @retval 0       成功
 * @retval -EINVAL dev 为空、pin 越界或 mode 非法
 * @note   本接口只配置 EXTI 触发源。使能后必须保证挂起位被及时清除：
 *         推荐在板级 EXTI 中断服务程序里根据 gpio_port_irq_status() 的结果
 *         逐个调用 gpio_pin_irq_ack()，否则 EXTI 会反复进入中断。
 *         对应 IRQn 的 NVIC 使能和优先级也由板级代码负责（驱动不碰 NVIC）。
 */
static inline int gpio_pin_interrupt_configure(const struct device *dev, gpio_pin_t pin,
                                               gpio_int_mode_t mode)
{
    if ((dev == NULL) || (dev->api == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->pin_interrupt_configure(dev, pin, mode);
}

/**
 * @brief  清除引脚的 EXTI 挂起标志
 * @retval 0      成功
 * @retval -EINVAL dev 为空或 pin 越界
 * @note   板级 ISR 示例：
 *             void EXTI9_5_IRQHandler(void)
 *             {
 *                 gpio_mask_t pending = 0;
 *                 gpio_port_irq_status(dev_btn, &pending);
 *                 if ((pending & (1U << 8)) != 0U) {
 *                     btn_on_edge();
 *                     gpio_pin_irq_ack(dev_btn, 8);
 *                 }
 *             }
 */
static inline int gpio_pin_irq_ack(const struct device *dev, gpio_pin_t pin)
{
    if ((dev == NULL) || (dev->api == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->pin_irq_ack(dev, pin);
}

/**
 * @brief  读整个端口的输入电平
 * @param  value 出参：bit n 对应 pin n 的电平
 * @retval 0      成功
 * @retval -EINVAL dev/value 为空
 * @note   返回 IDR 原值；仅低 GPIO_PINS_PER_PORT 位有意义。
 */
static inline int gpio_port_get(const struct device *dev, gpio_mask_t *value)
{
    if ((dev == NULL) || (dev->api == NULL) || (value == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->port_get(dev, value);
}

/**
 * @brief  置位端口内 mask 指定的引脚
 * @param  mask 位掩码，bit n 对应 pin n；高位必须为 0
 * @retval 0      成功
 * @retval -EINVAL dev 为空或 mask 超出 GPIO_PINS_PER_PORT
 * @note   单次 BSRR 写，原子。
 */
static inline int gpio_port_set_bits(const struct device *dev, gpio_mask_t mask)
{
    if ((dev == NULL) || (dev->api == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->port_set_bits(dev, mask);
}

/**
 * @brief  清零端口内 mask 指定的引脚
 * @param  mask 位掩码，bit n 对应 pin n；高位必须为 0
 * @retval 0      成功
 * @retval -EINVAL dev 为空或 mask 超出 GPIO_PINS_PER_PORT
 * @note   单次 BSRR 写，原子。
 */
static inline int gpio_port_clear_bits(const struct device *dev, gpio_mask_t mask)
{
    if ((dev == NULL) || (dev->api == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->port_clear_bits(dev, mask);
}

/**
 * @brief  翻转端口内 mask 指定的引脚
 * @param  mask 位掩码，bit n 对应 pin n；高位必须为 0
 * @retval 0      成功
 * @retval -EINVAL dev 为空或 mask 超出 GPIO_PINS_PER_PORT
 * @note   读改写 ODR，非原子。
 */
static inline int gpio_port_toggle_bits(const struct device *dev, gpio_mask_t mask)
{
    if ((dev == NULL) || (dev->api == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->port_toggle_bits(dev, mask);
}

/**
 * @brief  读端口的 EXTI 挂起位
 * @param  pending 出参：bit n 表示 pin n 有未处理的中断
 * @retval 0      成功
 * @retval -EINVAL dev/pending 为空
 * @note   驱动需按 SYSCFG->EXTICR 过滤：只报告当前映射到本端口的 EXTI 线，
 *         否则多个 GPIO 设备会报出同一批 pending 位。
 */
static inline int gpio_port_irq_status(const struct device *dev, gpio_mask_t *pending)
{
    if ((dev == NULL) || (dev->api == NULL) || (pending == NULL)) {
        return -EINVAL;
    }
    return ((const struct gpio_driver_api *)dev->api)->port_irq_status(dev, pending);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DRIVERS_GPIO_H_ */

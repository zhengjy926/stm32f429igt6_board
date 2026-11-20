/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_H_
#define DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct device;

struct device_state {
    uint8_t init_result;
    bool initialized;
};

/** Device operations */
struct device_ops {
	int (*init)(const struct device *dev);
    int (*deinit)(const struct device *dev);
};

struct device {
	const char *name;
    const void *config;
    const void *api;
    struct device_state *state;
    void *data;
	struct device_ops ops;
};

/*
 * 设备定义宏。
 *
 * 注意 1：宏形参带尾下划线（name_ / config_ / ...）。宏体里的 .name / .config
 *         等指定初始化器中的字段名同样是预处理记号，如果形参直接叫 name，
 *         预处理器会把 .name 换成实参，生成 ".gpioa = ..." 这种非法代码。
 * 注意 2：name_ 传字符串字面量，例如 DEVICE_DEFINE(gpioa, "GPIOA", ...)。
 * 注意 3：state 由宏自行分配（z_devstate_<id>），不接受外部传入；原版有一个
 *         被忽略的 state 形参，传了也不生效，易误导，已删除。
 * 注意 4：定义前先写一条 extern 声明：C++ 下 const 对象默认内部链接，没有它
 *         跨 TU 的 DEVICE_DECLARE/DEVICE_GET 会链接失败。
 * 注意 5：宏体最后一条是"不带头分号的声明"，由调用处的分号收尾，
 *         这样 DEVICE_DEFINE(...); 不会多出一条空声明（-Wextra-semi）。
 */
#define DEVICE_DEFINE(id, name_, config_, api_, data_, init_, deinit_)  \
	static struct device_state z_devstate_##id = {                     \
		.init_result = 0,                                              \
		.initialized = false,                                          \
	};                                                                 \
	extern const struct device z_device_##id;                          \
	const struct device z_device_##id = {                              \
		.name = name_,                                                 \
		.config = config_,                                             \
		.api = api_,                                                   \
		.state = &z_devstate_##id,                                     \
		.data = data_,                                                 \
		.ops = {                                                       \
			.init = init_,                                             \
			.deinit = deinit_,                                         \
		},                                                             \
	}

#define DEVICE_NAME_GET(dev_id) z_device_##dev_id
#define DEVICE_DECLARE(dev_id) extern const struct device DEVICE_NAME_GET(dev_id)
#define DEVICE_GET(dev_id) (&DEVICE_NAME_GET(dev_id))

bool device_is_ready(const struct device *dev);
int device_init(const struct device *dev);
int device_deinit(const struct device *dev);

#ifdef __cplusplus
}
#endif
#endif /* DEVICE_H_ */

#include "device.h"
#include "err_def.h"

bool device_is_ready(const struct device *dev)
{
    if ((dev == NULL) || (dev->state == NULL)) {
        return false;
    }
	return dev->state->initialized;
}

static int do_device_init(const struct device *dev)
{
	int rc = 0;

	if (dev->ops.init != NULL) {
		rc = dev->ops.init(dev);

		if (rc != 0) {
			// __ASSERT(rc >= -UINT8_MAX && rc < 0, "device %s init: invalid error (%d)",
			// 	 dev->name, rc);

			if (rc < 0) {
				rc = -rc;
			}

			if (rc > UINT8_MAX) {
				rc = UINT8_MAX;
			}
		}

		dev->state->init_result = rc;
	}

	if (rc == 0) {
		dev->state->initialized = true;
	}

	return -rc;
}

int device_init(const struct device *dev)
{
	if ((dev == NULL) || (dev->state == NULL)) {
		return -EINVAL;
	}

	if (dev->state->initialized) {
		return -EALREADY;
	}
	return do_device_init(dev);
}

int device_deinit(const struct device *dev)
{
	int ret;

	if ((dev == NULL) || (dev->state == NULL)) {
		return -EINVAL;
	}

	if (!dev->state->initialized) {
		return -EPERM;
	}

	if (dev->ops.deinit == NULL) {
		return -ENOTSUP;
	}

	ret = dev->ops.deinit(dev);
	if (ret < 0) {
		return ret;
	}

	dev->state->initialized = false;

	return 0;
}

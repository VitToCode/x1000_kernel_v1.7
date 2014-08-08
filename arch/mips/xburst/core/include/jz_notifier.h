#ifndef _JZ_NOTIFIER_H_
#define _JZ_NOTIFIER_H_

#include <linux/notifier.h>

/* Hibernation and suspend events
*/

enum jz_notif_cmd{
	JZ_CMD_START = 0,
	JZ_POST_HIBERNATION, /* Hibernation finished */
	JZ_CMD_END
};
enum {
	NOTEFY_PROI_START=1,
	NOTEFY_PROI_HIGH,
	NOTEFY_PROI_NORMAL,
	NOTEFY_PROI_LOW,
	NOTEFY_PROI_END
};

struct jz_notifier {
	struct notifier_block nb;
	int (*jz_notify)(struct jz_notifier *notify);
	int level;
	enum jz_notif_cmd msg;
};

int jz_notifier_register(struct jz_notifier *notify);
int jz_notifier_unregister(struct jz_notifier *notify);
int jz_notifier_call(unsigned long val, void *v);

#endif /* _JZ_NOTIFIER_H_ */

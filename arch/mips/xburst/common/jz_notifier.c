#include <jz_notifier.h>

static BLOCKING_NOTIFIER_HEAD(jz_notifier_chain);

static int jz_notifier(struct notifier_block *nb, unsigned long cmd, void *data)
{
	struct jz_notifier *jz_nb = container_of(nb,struct jz_notifier,nb);
	if(jz_nb->msg == cmd) {
		if(data)
			*(int *)data = jz_nb->jz_notify(jz_nb);
		else
			jz_nb->jz_notify(jz_nb);
	}
	return 0;
}

int jz_notifier_register(struct jz_notifier *notify)
{
	if((notify->level < NOTEFY_PROI_START) && (notify->level >= NOTEFY_PROI_END))
	{
		printk("notify level can not support this %d\n",notify->level);
		dump_stack();
		return -1;
	}
	if((int)notify->msg >= JZ_CMD_END && (int)notify->msg <= JZ_CMD_START)
	{
		printk("notify msg can not support this %d\n",notify->msg);
		dump_stack();
		return -1;
	}
	if(notify->jz_notify == NULL)
	{
		printk("notify function(jz_notify) cand not support NULL\n");
		dump_stack();
		return -1;
	}
	notify->nb.priority = notify->level;
	notify->nb.notifier_call = jz_notifier;
	return blocking_notifier_chain_register(&jz_notifier_chain, &notify->nb);
}

int jz_notifier_unregister(struct jz_notifier *notify)
{
	return blocking_notifier_chain_unregister(&jz_notifier_chain, &notify->nb);
}
int jz_notifier_call(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&jz_notifier_chain, val, v);
}

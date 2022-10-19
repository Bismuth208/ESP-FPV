#ifndef _ASYNC_PRINTF_CONF_H
#define _ASYNC_PRINTF_CONF_H


#ifndef ASYNC_PRINTF_MAX_ITEMS
/**
 * Amount of items to be held/stored with buffer.
 * Note this value ALWAYS should be the power of 2 for correct work of circular buffer!
 */
#define ASYNC_PRINTF_MAX_ITEMS (512)
#endif

#endif /* _ASYNC_PRINTF_CONF_H */
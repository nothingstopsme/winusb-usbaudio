#ifndef __LOG_H__
#define __LOG_H__

#include <cstdio>


#ifdef LOG_QUITE
	#define LOGD(fmt, ...)
	#define LOGI(fmt, ...) 
	#define LOGW(fmt, ...)
	#define LOGE(fmt, ...)
#else
#define AUTO_LINEBREAK(prefix, fmt) (fmt[strlen(fmt)-1] == '\r') ? prefix fmt : prefix fmt "\n"


	#ifndef NDEBUG
# define LOGD(fmt, ...) printf(AUTO_LINEBREAK("[DEBUG] ", fmt), ##__VA_ARGS__)
	#else
	# define LOGD(fmt, ...)
	#endif

#define LOGI(fmt, ...) printf(AUTO_LINEBREAK("[INFO] ", fmt), ##__VA_ARGS__)
#define LOGW(fmt, ...) printf(AUTO_LINEBREAK("[WARN] ", fmt), ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, AUTO_LINEBREAK("[ERROR] ", fmt), ##__VA_ARGS__)

#endif



#endif

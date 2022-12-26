#ifndef __ONIX_STDARG_HH__
#define __ONIX_STDARG_HH__

typedef char* va_list;

#define va_start(ap, v) (ap = (va_list)&v + sizeof(char*))
#define va_arg(ap, t) (*(t *)((ap += sizeof(char *)) - sizeof(char*)))
#define va_end(ap) (ap = (va_list)0)

#endif 
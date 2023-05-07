#ifndef MACRO_UTILS_H
#define MACRO_UTILS_H

#define _STRINGFY(expr) #expr
#define STRINGFY(expr) _STRINGFY(expr)

#define _JOIN(left, right) left##right
#define JOIN(left, right) _JOIN(left, right)

#endif /* MACRO_UTILS_H */

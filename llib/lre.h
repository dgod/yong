#pragma once

int l_re_test(const char *re,const char *s);
LPtrArray *l_re_match(const char *re,const char *s);
char *l_re_replace(const char *re,const char *s,const char *replacement);

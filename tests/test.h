#ifndef TEST_H
#define TEST_H
#include <stdio.h>
static int t_passed = 0, t_failed = 0;
#define CHECK(cond) do { if (cond) t_passed++; else { t_failed++; \
  printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long _a = (long long)(a), _b = (long long)(b); \
  if (_a == _b) t_passed++; else { t_failed++; \
  printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)
#define RUN(fn) do { fn(); } while (0)
#define TEST_SUMMARY() (printf("%d passed, %d failed\n", t_passed, t_failed), t_failed ? 1 : 0)
#endif

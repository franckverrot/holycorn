#include "holycorn_redis.h"

void mrb_holycorn_redis_gem_init(mrb_state *mrb) {
  struct RClass *holycorn_redis;

  holycorn_redis = mrb_define_class(mrb, "HolycornRedis", mrb->object_class);
}

void mrb_holycorn_redis_gem_final(mrb_state *mrb) {
}

MODULES = holycorn
MODULE_big = holycorn

PG_CPPFLAGS = -g -Ivendor/mruby/include -lm
EXTENSION = holycorn
SHLIB_LINK = vendor/mruby/build/i686-pc-linux-gnu/lib/libmruby.a vendor/mruby/build/i686-pc-linux-gnu/mrbgems/mruby-redis/hiredis/libhiredis.a
OBJS = holycorn.o
DATA = holycorn--1.0.sql
PGFILEDESC = "holycorn - Ruby foreign data wrapper provider"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

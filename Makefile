MODULES = holycorn
MODULE_big = holycorn

PG_CPPFLAGS = -g -Ivendor/mruby/include
EXTENSION = holycorn
OBJS = holycorn.o vendor/mruby/build/host/lib/libmruby.a vendor/mruby/build/host/mrbgems/mruby-redis/hiredis/libhiredis.a
DATA = holycorn--1.0.sql
PGFILEDESC = "holycorn - Ruby foreign data wrapper provider"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

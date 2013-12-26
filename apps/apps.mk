
APPS_MK = $(shell find $(APPS) -mindepth 2 -type f -name *.mk)

apps_all: helloworld \
          lda \
          tools

.PHONY: apps_all

include $(APPS_MK)

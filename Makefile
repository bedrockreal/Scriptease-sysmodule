.PHONY: all clean

all:
	@$(MAKE) -C sys-botbase/

sd:
	@$(MAKE) sd -C sys-botbase/

clean:
	@$(MAKE) clean -C sys-botbase/
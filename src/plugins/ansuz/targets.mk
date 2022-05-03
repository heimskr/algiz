plugin/ansuz.$(SHARED_EXT): $(patsubst %.cpp,%.o,$(addprefix src/plugins/ansuz/,$(filter %.cpp,$(shell ls "src/plugins/ansuz")))) include/plugins/ansuz/resources.h
	$(COMPILER) $(SHARED_FLAG) $(filter %.o,$+) -o $@ $(LDFLAGS) -Wl,-undefined,dynamic_lookup

ansuz_res: include/plugins/ansuz/resources.h

ansuz_clean:
	rm -f include/plugins/ansuz/resources.h

include/plugins/ansuz/resources.h: res/ansuz/index.t res/ansuz/style.css res/ansuz/bootstrap.min.css \
res/ansuz/bootstrap-utilities.min.css res/ansuz/not_loaded.t res/ansuz/unloaded.t
	echo "#include <cstdlib>" > $@
	bin2c ansuz_index < res/ansuz/index.t | tail -n +2 >> $@
	bin2c ansuz_css < res/ansuz/style.css | tail -n +2 >> $@
	bin2c ansuz_bootstrap_css < res/ansuz/bootstrap.min.css | tail -n +2 >> $@
	bin2c ansuz_bootstrap_util_css < res/ansuz/bootstrap-utilities.min.css | tail -n +2 >> $@
	bin2c ansuz_not_loaded < res/ansuz/not_loaded.t | tail -n +2 >> $@
	bin2c ansuz_unloaded < res/ansuz/unloaded.t | tail -n +2 >> $@

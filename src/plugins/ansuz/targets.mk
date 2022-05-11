plugin/ansuz.$(SHARED_EXT): $(patsubst %.cpp,%.o,$(addprefix src/plugins/ansuz/,$(filter %.cpp,$(shell ls "src/plugins/ansuz")))) include/plugins/ansuz/resources.h
	$(COMPILER) $(SHARED_FLAG) $(filter %.o,$+) -o $@ $(LDFLAGS) -Wl,-undefined,dynamic_lookup

ansuz_res: include/plugins/ansuz/resources.h

ansuz_clean:
	rm -f include/plugins/ansuz/resources.h

include/plugins/ansuz/resources.h: res/ansuz/index.t res/ansuz/style.css res/ansuz/message.t res/ansuz/unloaded.t \
res/ansuz/load.t res/ansuz/edit_config.t
	echo "#include <cstdlib>" > $@
	bin2c ansuz_index < res/ansuz/index.t | tail -n +2 >> $@
	bin2c ansuz_css < res/ansuz/style.css | tail -n +2 >> $@
	bin2c ansuz_message < res/ansuz/message.t | tail -n +2 >> $@
	bin2c ansuz_unloaded < res/ansuz/unloaded.t | tail -n +2 >> $@
	bin2c ansuz_load < res/ansuz/load.t | tail -n +2 >> $@
	bin2c ansuz_edit_config < res/ansuz/edit_config.t | tail -n +2 >> $@

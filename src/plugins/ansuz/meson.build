ansuz_resources = custom_target('ansuz_gen',
	input: ['../../../res/ansuz/index.t'],
	output: ['ansuz_resources.h'],
	command: ['./gen.sh', '@INPUT@', '@OUTPUT@'])

ansuz_sources = ['Ansuz.cpp']

ansuz_plugin = shared_module('ansuz_plugin', ansuz_sources,
	dependencies: algiz_deps,
	link_args: link_args,
	install: true,
	install_dir: 'plugin',
	include_directories: inc_dirs)

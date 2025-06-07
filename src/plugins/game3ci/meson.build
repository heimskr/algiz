game3ci_plugin = shared_module('game3ci_plugin', ['Game3CI.cpp'],
	dependencies: algiz_deps + [
		dependency('libcurl'),
		dependency('curlpp'),
	],
	link_args: link_args,
	install: true,
	install_dir: 'plugin',
	include_directories: inc_dirs)

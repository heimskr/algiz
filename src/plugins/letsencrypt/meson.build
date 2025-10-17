acme_lw_options = cmake.subproject_options()
acme_lw_options.add_cmake_defines({'BUILD_SHARED_LIBS': true})
acme_lw = cmake.subproject('acme-lw', options: acme_lw_options)

letsencrypt_plugin = shared_module('letsencrypt_plugin', ['LetsEncrypt.cpp'],
	dependencies: algiz_deps + acme_lw.dependency('acme_lw'),
	link_args: link_args,
	install: true,
	install_dir: 'plugin',
	include_directories: inc_dirs)

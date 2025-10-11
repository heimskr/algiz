cmake = import('cmake')

algiz_sources = run_command('grabber.sh', check: true).stdout().strip().split('\n')

inja = cmake.subproject('inja')
json = cmake.subproject('json')
wahtwo = subproject('wahtwo')

algiz_deps = [
	dependency('openssl'),
	dependency('threads'),
	dependency('libevent'),
	dependency('libevent_pthreads'),
	dependency('libevent_openssl'),
	dependency('llvm'),
	wahtwo.get_variable('wahtwo'),
	inja.dependency('inja'),
	json.dependency('nlohmann_json')
]

inc_dirs = [
	include_directories('..' / 'include'),
]

link_with = [wahtwo.get_variable('wahtwo_lib')]

link_args = ['-lclang', '-lclang-cpp']

if get_option('enable_geoip')
	inc_dirs += include_directories('../geolite2++/src-lib')
	algiz_deps += [
		cxx.find_library('geolite2++', dirs: meson.current_source_dir() + '/../geolite2++/build/src-lib'),
		dependency('libmaxminddb'),
	]
endif

subdir('plugins')

algiz_sources += ansuz_resources

executable('algiz', algiz_sources,
	dependencies: algiz_deps,
	export_dynamic: true,
	link_with: link_with,
	link_args: link_args,
	install: true,
	include_directories: [inc_dirs])

project('sim_cpp', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

include_dir = include_directories('include')

subdir('src')
subdir('tests')

executable(
  'sim',
  'main.cpp',
  sources,
  install : true,
  include_directories : include_dir
)

executable(
  'channel',
  'channel.cpp',
  sources,
  install : true,
  include_directories : include_dir
)

executable(
  'peer',
  'peer.cpp',
  sources,
  install : true,
  include_directories : include_dir
)

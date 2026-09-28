project('sim_cpp', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

includeDir = include_directories('include')

subdir('src')
subdir('tests')

executable('sim_cpp',
           'main.cpp',
           sources,
           install : true,
           include_directories : includeDir)

project('sim_cpp', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

include_dir = include_directories('include')
config_include_dir = include_directories('include/config')

subdir('src')
subdir('src/gui')
subdir('src/config')
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
  include_directories : [include_dir]
)

executable(
  'peer',
  'peer.cpp',
  sources,
  config_sources,
  install : true,
  include_directories : [include_dir, config_include_dir]
)

# Interface gráfica (opcional): fica no mesmo diretório de `channel` e `peer`,
# pois é onde ela procura os executáveis que cria
gtkmm_dep = dependency('gtkmm-4.0', required : false)

if gtkmm_dep.found()
  executable(
    'gui',
    gui_sources,
    sources,
    config_sources,
    dependencies : gtkmm_dep,
    install : true,
    include_directories : [include_dir, include_directories('include/gui'), config_include_dir]
  )
else
  message('gtkmm-4.0 não encontrado: a interface gráfica não será compilada')
endif

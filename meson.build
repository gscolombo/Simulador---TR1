project('sim_cpp', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

include_dir = include_directories('include')

subdir('src')
subdir('src/gui')
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

# Interface gráfica (opcional): fica no mesmo diretório de `channel` e `peer`,
# pois é onde ela procura os executáveis que cria
gtkmm_dep = dependency('gtkmm-4.0', required : true)

if gtkmm_dep.found()
  executable(
    'gui',
    gui_sources,
    sources,
    dependencies : gtkmm_dep,
    install : true,
    include_directories : [include_dir, include_directories('include/gui')]
  )
else
  message('gtkmm-4.0 não encontrado: a interface gráfica não será compilada')
endif

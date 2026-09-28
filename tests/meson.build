include_dir = include_directories('../include')
src_dir = include_directories('../src')

test_modem = executable(
    'test_modem',
    'test_modem.cpp',
    sources,
    include_directories : [include_dir, src_dir]
)

test_baseband_modem = executable(
    'test_baseband_modem',
    'test_baseband_modem.cpp',
    sources,
    include_directories : [include_dir, src_dir]
)

test_passband_modem = executable(
    'test_passband_modem',
    'test_passband_modem.cpp',
    sources,
    include_directories : [include_dir, src_dir]
)

test_integration = executable(
    'test_integration',
    'test_integration.cpp',
    sources,
    include_directories : [include_dir, src_dir]
)

test_fullduplex = executable(
    'test_fullduplex',
    'test_fullduplex.cpp',
    sources,
    include_directories : [include_dir, src_dir]
)

test('modem', test_modem)
test('baseband_modem', test_baseband_modem)
test('passband_modem', test_passband_modem)
test('integration', test_integration)
test('fullduplex', test_fullduplex)
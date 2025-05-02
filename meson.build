project('unzboot', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

glibdep = dependency('glib-2.0')
zdep = dependency('zlib')
zstddep = dependency('libzstd')

executable('unzboot', 'unzboot.c',
  dependencies: [glibdep, zdep, zstddep],
  install : true)

# --- Add Tests ---
test_deps = [glibdep, zdep, zstddep]

test_sources = ['test_unzboot.c'] # Includes unzboot.c internally

test_exe = executable('test_unzboot',
                      test_sources,
                      dependencies : test_deps)

test('Unzboot Tests', test_exe)

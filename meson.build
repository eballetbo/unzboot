project('unzboot', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

glibdep = dependency('glib-2.0')
zdep = dependency('zlib')
zstddep = dependency('libzstd')

exe = executable('unzboot', 'unzboot.c',
  dependencies: [glibdep, zdep, zstddep],
  install : true)

test('basic', exe)

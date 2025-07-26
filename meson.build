# SPDX-License-Identifier: Apache-2.0
project('json-diff', 'c',
  version : '1.0.0',
  license : 'Apache-2.0',
  default_options : [
    'c_std=c11',
    'warning_level=3',
    'werror=true',
    'optimization=2',
    'debug=true'
  ]
)

# Dependencies
cjson_dep = dependency('libcjson', required : true)
math_dep = meson.get_compiler('c').find_library('m', required : false)

# Library
json_diff_lib = static_library('jsondiff',
  'json_diff.c',
  dependencies : [cjson_dep, math_dep],
  install : true,
  install_dir : get_option('libdir')
)

# Install header
install_headers('json_diff.h')

# Test executable
test_exe = executable('test_json_diff',
  'test_json_diff.c',
  link_with : json_diff_lib,
  dependencies : [cjson_dep, math_dep],
  install : false
)

# Register test
test('json_diff_test', test_exe)

# Declare dependency for other projects
json_diff_dep = declare_dependency(
  link_with : json_diff_lib,
  include_directories : include_directories('.'),
  dependencies : [cjson_dep, math_dep]
)

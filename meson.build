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

# Fuzzing support (only if compiler supports it)
cc = meson.get_compiler('c')
if cc.has_argument('-fsanitize=fuzzer')
  fuzz_exe = executable('fuzz_json_diff',
    'fuzz_json_diff.c',
    link_with : json_diff_lib,
    dependencies : [cjson_dep, math_dep],
    c_args : ['-fsanitize=fuzzer', '-g'],
    link_args : ['-fsanitize=fuzzer'],
    install : false
  )
  
  # Add a custom target for running the fuzzer
  run_target('fuzz',
    command : ['sh', '-c', 'mkdir -p corpus && echo \'{"test": 1}\' > corpus/simple1.json && echo \'{"test": 2}\' > corpus/simple2.json && echo \'{"arr": [1,2,3]}\' > corpus/array1.json && echo \'{"arr": [2,3,4]}\' > corpus/array2.json && ' + fuzz_exe.full_path() + ' -max_total_time=60 corpus/']
  )
  
  # Add a longer fuzzing session target
  run_target('fuzz-long',
    command : ['sh', '-c', 'mkdir -p corpus && echo \'{"test": 1}\' > corpus/simple1.json && echo \'{"test": 2}\' > corpus/simple2.json && echo \'{"arr": [1,2,3]}\' > corpus/array1.json && echo \'{"arr": [2,3,4]}\' > corpus/array2.json && ' + fuzz_exe.full_path() + ' -max_total_time=300 corpus/']
  )
  
  # Add a target to run fuzzer with custom arguments
  run_target('fuzz-custom',
    command : ['sh', '-c', 'mkdir -p corpus && echo \'{"test": 1}\' > corpus/simple1.json && echo \'{"test": 2}\' > corpus/simple2.json && echo \'{"arr": [1,2,3]}\' > corpus/array1.json && echo \'{"arr": [2,3,4]}\' > corpus/array2.json && ' + fuzz_exe.full_path() + ' corpus/']
  )
endif

# Declare dependency for other projects
json_diff_dep = declare_dependency(
  link_with : json_diff_lib,
  include_directories : include_directories('.'),
  dependencies : [cjson_dep, math_dep]
)

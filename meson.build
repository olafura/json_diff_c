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
# end of bench-jsmn target
)

# End-to-end parse+diff pipeline micro-benchmark
bench_pipeline_exe = executable('bench_pipeline',
  'tests/bench_pipeline.c',
  link_with : json_diff_lib,
  dependencies : [cjson_dep],
  install : false
)
run_target('bench-pipeline',
  command : [bench_pipeline_exe]
)

# Dependencies
cjson_dep = dependency('libcjson', required : true)
math_dep = meson.get_compiler('c').find_library('m', required : false)

# Library
json_diff_lib = static_library('jsondiff',
  'src/json_diff.c', 'src/parse_jsmn.c', 'src/jsmn_tree.c', 'src/jsmn.c',
  dependencies : [cjson_dep, math_dep],
  install : true,
  install_dir : get_option('libdir')
)

# Install header
install_headers('src/json_diff.h')

# Test executable
test_exe = executable('test_json_diff',
  'tests/test_json_diff.c',
  link_with : json_diff_lib,
  dependencies : [cjson_dep, math_dep],
  install : false
)

# Register test
test('json_diff_test', test_exe)

# Profile test executable
profile_exe = executable('test_profile',
  'tests/test_profile.c',
  link_with : json_diff_lib,
  dependencies : [cjson_dep, math_dep],
  install : false
)

# Add a custom target for running profile tests
run_target('profile',
  command : [profile_exe]
)

# Medium diff benchmark (isolated timing of json_diff)
bench_medium_exe = executable('bench_medium',
  'tests/bench_medium.c',
  link_with : json_diff_lib,
  dependencies : [cjson_dep, math_dep],
  install : false
)
run_target('bench-medium',
  command : [bench_medium_exe]
)

# Parser performance micro‑benchmark
bench_parse_exe = executable('bench_parse',
  'tests/bench_parse.c',
  dependencies : [cjson_dep],
  install : false
)
## bench-parse run_target removed due to Meson introspect bug; run manually

# JSMN parse benchmark (vs. cJSON)
bench_jsmn_exe = executable('bench_jsmn',
  'tests/bench_jsmn.c', 'src/jsmn.c',
  install : false
)
## bench-jsmn run_target removed; run manually


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

# Clang-tidy support
clang_tidy = find_program('clang-tidy', required : false)
if clang_tidy.found()
  # Add clang-tidy target
  run_target('clang-tidy',
    command : [
      clang_tidy,
      '-p', meson.current_build_dir(),
      '--config-file=' + join_paths(meson.current_source_dir(), '.clang-tidy'),
      'src/json_diff.c',
      'tests/test_json_diff.c',
      'tests/test_profile.c',
      'fuzz_json_diff.c'
    ],
    depends : [json_diff_lib, test_exe, profile_exe]
  )
  
  # Add clang-tidy fix target
  run_target('clang-tidy-fix',
    command : [
      clang_tidy,
      '-p', meson.current_build_dir(),
      '--config-file=' + join_paths(meson.current_source_dir(), '.clang-tidy'),
      '--fix',
      'src/json_diff.c',
      'tests/test_json_diff.c',
      'tests/test_profile.c',
      'fuzz_json_diff.c'
    ],
    depends : [json_diff_lib, test_exe, profile_exe]
  )
endif

# Static analysis target
run_target('analyze',
  command : [
    'sh', '-c',
    'if command -v clang-tidy >/dev/null 2>&1; then ' +
    'echo "Running clang-tidy..."; ' +
    'meson compile -C ' + meson.current_build_dir() + ' clang-tidy; ' +
    'fi; ' +
    'if command -v cppcheck >/dev/null 2>&1; then ' +
    'echo "Running cppcheck..."; ' +
    'cppcheck --enable=all --std=c11 --platform=unix64 ' +
    '--suppress=missingIncludeSystem --suppress=unusedFunction ' +
    '--suppress=unmatchedSuppression --inline-suppr ' +
    'src/ .; ' +
    'fi'
  ]
)

# Declare dependency for other projects
json_diff_dep = declare_dependency(
  link_with : json_diff_lib,
  include_directories : include_directories('src'),
  dependencies : [cjson_dep, math_dep]
)

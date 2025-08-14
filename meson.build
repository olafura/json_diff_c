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
thread_dep = dependency('threads', required : false)

# Global hardening flags
hardening_args = [
  '-fstack-protector-strong',
  '-D_FORTIFY_SOURCE=2',
  '-Wformat-security',
  '-Werror=format-security',
  '-fPIE'
]
add_project_arguments(hardening_args, language: 'c')
add_project_link_arguments(['-Wl,-z,relro', '-Wl,-z,now', '-pie'], language: 'c')

# Composite dependencies
if thread_dep.found()
  base_deps = [cjson_dep, math_dep, thread_dep]
else
  base_deps = [cjson_dep, math_dep]
endif

# Library
json_diff_lib = static_library('jsondiff',
  'src/json_diff.c',
  dependencies : base_deps,
  install : true,
  install_dir : get_option('libdir')
)

# Install header
install_headers('src/json_diff.h')

# Test executable
test_exe = executable('test_json_diff',
  'tests/test_json_diff.c',
  link_with : json_diff_lib,
  dependencies : base_deps,
  install : false
)

# Register test
test('json_diff_test', test_exe, workdir : meson.current_source_dir())

# Security test executable
security_test_exe = executable('test_security',
  'tests/test_security.c',
  link_with : json_diff_lib,
  dependencies : base_deps,
  install : false
)

# Register security test
test('security_test', security_test_exe, workdir : meson.current_source_dir())

# jsondiffpatch compatibility test executable
js_compat_test_exe = executable('test_js_compat',
  'tests/test_js_compat.c',
  link_with : json_diff_lib,
  dependencies : base_deps,
  install : false
)

# Register jsondiffpatch compatibility test
test('js_compat_test', js_compat_test_exe, workdir : meson.current_source_dir())

# Node.js comparison test executable (optional, requires Node.js)
node_compare_test_exe = executable('test_node_compare',
  'tests/test_node_compare.c',
  link_with : json_diff_lib,
  dependencies : base_deps,
  install : false
)

# Register Node.js comparison test (runs but may skip if Node.js unavailable)
test('node_compare_test', node_compare_test_exe, workdir : meson.current_source_dir())

# Simple jsondiffpatch compatibility test (for debugging)
simple_js_compat_test_exe = executable('test_js_compat_simple',
  'tests/test_js_compat_simple.c',
  link_with : json_diff_lib,
  dependencies : base_deps,
  install : false
)

# Behavior verification test 
behavior_verification_exe = executable('test_behavior_verification',
  'tests/test_behavior_verification.c',
  link_with : json_diff_lib,
  dependencies : base_deps,
  install : false
)

# Register behavior verification test
test('behavior_verification_test', behavior_verification_exe, workdir : meson.current_source_dir())

# Property-based test executable using theft
# Only build if theft library is available
theft_lib_path = join_paths(meson.current_source_dir(), 'vendor/theft/build')
theft_lib = meson.get_compiler('c').find_library('theft',
  dirs: [theft_lib_path],
  required: false)

theft_inc = include_directories('vendor/theft/inc')

if theft_lib.found()
  theft_test_exe = executable('test_theft_properties',
    'tests/test_theft_properties.c',
    link_with : json_diff_lib,
    dependencies : [base_deps, theft_lib],
    include_directories : theft_inc,
    install : false
  )

  # Register property-based test with timeout and reduced trials for CI
  test('theft_properties_test', theft_test_exe,
    workdir : meson.current_source_dir(),
    timeout : 120,  # 2 minutes timeout
    env : ['THEFT_QUICK_TEST=1'])  # Environment variable to reduce trials

  # Theft-based fuzzing test executable
  theft_fuzz_exe = executable('test_theft_fuzzing',
    'tests/test_theft_fuzzing.c',
    link_with : json_diff_lib,
    dependencies : [base_deps, theft_lib],
    include_directories : theft_inc,
    install : false
  )

  # Register fuzzing test with timeout and reduced trials for CI
  test('theft_fuzzing_test', theft_fuzz_exe,
    workdir : meson.current_source_dir(),
    timeout : 60,   # 1 minute timeout
    env : ['THEFT_QUICK_TEST=1'])  # Environment variable to reduce trials
else
  message('theft library not found, skipping property-based and fuzzing tests')
endif

# Profile test executable
profile_exe = executable('test_profile',
  'tests/test_profile.c',
  link_with : json_diff_lib,
  dependencies : base_deps,
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
  dependencies : base_deps,
  install : false
)
run_target('bench-medium',
  command : [bench_medium_exe]
)

# Parser performance micro‑benchmark
bench_parse_exe = executable('bench_parse',
  'tests/bench_parse.c',
  dependencies : base_deps,
  install : false
)
## bench-parse run_target removed due to Meson introspect bug; run manually


# Fuzzing support (only if compiler supports it)
cc = meson.get_compiler('c')
if cc.has_argument('-fsanitize=fuzzer')
  fuzz_exe = executable('fuzz_json_diff',
    'tests/fuzz_json_diff.c',
    link_with : json_diff_lib,
    dependencies : base_deps,
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
  include_directories : include_directories('src'),
  dependencies : [cjson_dep, math_dep]
)

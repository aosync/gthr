project('gthr', 'c')

ctx_dep = dependency('ctx', required : false)
if not ctx_dep.found()
   ctx_proj = subproject('ctx')
   ctx_dep = ctx_proj.get_variable('ctx_dep')
endif

vec_dep = dependency('vec', required : false)
if not vec_dep.found()
   vec_proj = subproject('vec')
   vec_dep = vec_proj.get_variable('vec_dep')
endif

spin_dep = dependency('spin', required : false)
if not spin_dep.found()
   spin_proj = subproject('spin')
   spin_dep = spin_proj.get_variable('spin_dep')
endif

all_deps = [ctx_dep, vec_dep, spin_dep]

gthr_inc = include_directories('.')
gthr_lib = static_library('gthr', 'gthr.c', include_directories : gthr_inc, dependencies : all_deps)
gthr_dep = declare_dependency(link_with : gthr_lib, include_directories : gthr_inc, dependencies : all_deps)

executable('gthrmain', 'main.c', dependencies : gthr_dep)
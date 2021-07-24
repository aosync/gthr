project('gthr', 'c')

ctx_dep = dependency('ctx', required : false)
if not ctx_dep.found()
   ctx_proj = subproject('ctx')
   ctx_dep = ctx_proj.get_variable('ctx_dep')
endif

gthr_inc = include_directories('.')
gthr_lib = static_library('gthr', 'gthr.c', include_directories : gthr_inc, dependencies : ctx_dep)
gthr_dep = declare_dependency(link_with : gthr_lib, include_directories : gthr_inc, dependencies : ctx_dep)

executable('gthrmain', 'main.c', dependencies : gthr_dep)
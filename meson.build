project('xrun', 'cpp', version : '2.0', license : 'MIT', default_options : ['c_std=c20', 'cpp_std=c++20'])
        
subdir('src')

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DDEBUG', language : 'cpp')
endif

executable('xrun', [xrun_files], dependencies : [xrun_deps])
executable('xserver', [xserver_files], dependencies : [xserver_deps])
executable('xworker', [xworker_files], dependencies : [xworker_deps])

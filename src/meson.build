eventtalker_src = [  
  'dbmanager.c', 
  'main.c',  
]

eventtalker_deps = [
  dependency('gtk+-3.0', version : '>=3.22'),
  dependency('sqlite3', version : '>= 3.27.2')
    
]

executable(
  'eventtalker', eventtalker_src,
  dependencies: eventtalker_deps, 
  gui_app: true
)

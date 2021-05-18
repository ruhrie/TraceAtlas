add_library(Hydra Hydra.c)
target_include_directories(Hydra PUBLIC include)

install(TARGETS Hydra LIBRARY DESTINATION lib)
add_executable(eib_replay replay_itch.cpp)
target_link_libraries(eib_replay PRIVATE eib_core)

# Standalone steady-state allocation guard (overrides global operator new).
add_executable(eib_zalloc_check zalloc_check.cpp)
target_link_libraries(eib_zalloc_check PRIVATE eib_core)

# Research driver: runs the signal strategy per fill model, emits a returns CSV.
add_executable(eib_research research_run.cpp)
target_link_libraries(eib_research PRIVATE eib_core)

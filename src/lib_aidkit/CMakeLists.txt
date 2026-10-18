# Qt-free core of AidKit:
add_library(AidKit_lib
	aidkit/enum_class.hpp
	aidkit/enum_class.cpp

	aidkit/concurrent/thread_shared.hpp
	aidkit/concurrent/thread_shared.cpp
	
	aidkit/concurrent/blocking_queue.hpp
	aidkit/concurrent/blocking_queue.cpp
)
setDefaultTargetOptions(AidKit_lib)

target_include_directories(AidKit_lib
	PUBLIC
		.
)
# AidKit_lib is now Qt-free! No External_lib_qt dependency.

# Qt-dependent AidKit utilities (only for lib_gui consumers):
add_library(AidKit_qt
	aidkit/qt/Strings.hpp
	aidkit/qt/Strings.cpp
)
setDefaultTargetOptions(AidKit_qt)

target_include_directories(AidKit_qt
	PUBLIC
		.
)
target_link_libraries(AidKit_qt
	PUBLIC
		AidKit_lib
		External_lib_qt
)

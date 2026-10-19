# Qt-free core of AidKit:
add_library(AidKit_lib
	aidkit/enum_class.hpp
	aidkit/enum_class.cpp

	aidkit/concurrent/thread_shared.hpp
	aidkit/concurrent/thread_shared.cpp

	aidkit/concurrent/blocking_queue.hpp
	aidkit/concurrent/blocking_queue.cpp
)
# This static archive is linked into the shared core on Linux.
set_target_properties(AidKit_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
setDefaultTargetOptions(AidKit_lib)

target_include_directories(AidKit_lib
	PUBLIC
		.
)
# AidKit_lib is now Qt-free! No External_lib_qt dependency.

# Qt-dependent AidKit utilities (only for lib_gui consumers):
if (BUILD_GUI)
	add_library(AidKit_qt
		aidkit/qt/Strings.hpp
		aidkit/qt/Strings.cpp
	)
	set_target_properties(AidKit_qt PROPERTIES POSITION_INDEPENDENT_CODE ON)
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
endif()

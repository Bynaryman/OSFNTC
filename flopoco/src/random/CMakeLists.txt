set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})

message(${CMAKE_MODULE_PATH})

FIND_PACKAGE(Boost COMPONENTS test_exec_monitor)
FIND_PACKAGE(GSL)

FIND_PATH(NTL_H NTL/RR.h )

FIND_LIBRARY(NTL_LIB NAMES ntl ntl.lib )

IF (NTL_H AND NTL_LIB)
    SET(NTL_FOUND TRUE)
	ADD_DEFINITIONS(-DHAVE_NTL)
	
	FIND_LIBRARY(GMP_LIB NAMES gmp gmp.lib )

	INCLUDE_DIRECTORIES(${NTL_H})
	LINK_DIRECTORIES(${NTL_LIB})
ENDIF (NTL_H AND NTL_LIB)

ADD_LIBRARY(flopoco_random_lib STATIC
	random_main.cpp
	urng/LutSrRng.cpp
)
target_link_libraries(flopoco_random_lib ${GMP_LIB})


#INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR} ${GSL_INCLUDE_DIR})
#LINK_DIRECTORIES ( ${Boost_LIBRARY_DIRS} ${GSL_LIBRARY_DIRS})

#add_executable(test_distributions distributions/test_distributions.cpp)
#target_link_libraries(test_distributions ${Boost_LIBRARIES})

#add_executable(test_optimisation utils/test_optimisation.cpp)
#target_link_libraries(test_optimisation ${Boost_LIBRARIES} ${GSL_LIBRARIES})

#add_executable(test_moment_correction moment_correction/test_moment_correction.cpp)
#target_link_libraries(test_moment_correction ${Boost_LIBRARIES} ${GSL_LIBRARIES} ${NTL_LIB} ${GMP_LIB})


#add_executable(gaussian_moment_correction_tables moment_correction/gaussian_moment_correction_tables.cpp)
#target_link_libraries(gaussian_moment_correction_tables ${Boost_LIBRARIES} ${GSL_LIBRARIES} ${NTL_LIB} ${GMP_LIB})

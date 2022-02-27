cmake_minimum_required(VERSION 3.20)

#set(Boost_USE_STATIC_LIBS FALSE)
#set(Boost_USE_MULTITHREADED TRUE)
#set(Boost_USE_STATIC_RUNTIME FALSE)
#set(Boost_ADDITIONAL_VERSIONS "${BOOST_VERSION}")

find_package(Boost 1.58.0 REQUIRED)

if(Boost_FOUND)
  set_target_properties(Boost::boost PROPERTIES IMPORTED_GLOBAL TRUE)
  set_target_properties(Boost::headers PROPERTIES IMPORTED_GLOBAL TRUE)
endif()

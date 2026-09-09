cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0167 NEW)   # 抑制警告

project(qipai_server)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# # ---------- 第三方库路径 ----------
# # Boost 根目录：解压根目录，内部直接有 boost/ 文件夹
set(Boost_ROOT "E:/cppsoft/boost_for_mingw")
# # 注意：CMake3.20 find_package(Boost) 识别的是 Boost_ROOT，不是 BOOST_ROOT(全大写)
# # 不要手动设置 BOOST_INCLUDEDIR / BOOST_LIBRARYDIR，find_package会自动推导
set(OPENSSL_ROOT_DIR "E:/OpenSSL-Win64")
set(mysql-concpp_DIR "E:/cppsoft/mysql-connector-full")
set(rabbitmq-c_DIR "E:/cppsoft/rabbitmq-c/install/lib/cmake/rabbitmq-c")
# set(Boost_IGNORE_DETECTED_COMPILER ON)

# # 查找Boost，需要的组件：log log_setup filesystem system
# find_package(Boost REQUIRED COMPONENTS log log_setup filesystem system)
# message(STATUS "Boost_INCLUDE_DIRS = ${Boost_INCLUDE_DIRS}")
# message(STATUS "Boost_LIBRARIES = ${Boost_LIBRARIES}")

# find_package(mysql-concpp REQUIRED COMPONENTS jdbc)
# find_package(rabbitmq-c CONFIG REQUIRED)

function(assign_folder_group)
    foreach(_source IN ITEMS ${ARGN})
        if (IS_ABSOLUTE "${_source}")
          file(RELATIVE_PATH _source_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_source}")
        else()
          set(_source_rel "${_source}")
        endif()
        get_filename_component(_source_path "${_source_rel}" PATH)
        string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
        if("${_source_path_msvc}" STREQUAL "." OR "${_source_path_msvc}" STREQUAL "")
          get_filename_component(extension "${_source}" EXT)
          if("${extension}" STREQUAL ".h")
            set(_source_path_msvc "include")
          else()
            set(_source_path_msvc "source")
          endif()
        endif()
        source_group("${_source_path_msvc}" FILES "${_source}")
    endforeach()
endfunction(assign_folder_group)

function(assign_source_group)
    foreach(_source IN ITEMS ${ARGN})
      get_filename_component(extension "${_source}" EXT)
      if("${extension}" STREQUAL ".h")
        set(_source_path_msvc "include")
      else()
        set(_source_path_msvc "source")
      endif()
      source_group("${_source_path_msvc}" FILES "${_source}")
    endforeach()
endfunction(assign_source_group)


function(update_cached name value)
  set("${name}" "${value}" CACHE INTERNAL "*** Internal ***" FORCE)
endfunction()

function(update_cached_list name)
  set(_tmp_list "${${name}}")
  list(APPEND _tmp_list "${ARGN}")
  list(REMOVE_DUPLICATES _tmp_list)
  update_cached(${name} "${_tmp_list}")
endfunction()

#首先清空缓存
# 3rdpart libraries
update_cached(3RDPART_LIBRARIES "")

# mysql libraries
update_cached(MYSQL_LIBRARIES "")

# rabbitmq libraries
update_cached(RABBITMQ_LIBRARIES "")

# Framework library
update_cached(FRAMEWORK_LIBRARY "")

# add 3rdpart to this project
add_subdirectory(3rdpart)

find_package(Boost REQUIRED COMPONENTS log log_setup filesystem system)
message(STATUS "Boost_FOUND = ${Boost_FOUND}")
message(STATUS "Boost_INCLUDE_DIRS = ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARIES = ${Boost_LIBRARIES}")

find_package(OpenSSL REQUIRED)
message(STATUS "OPENSSL_INCLUDE_DIR = ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OPENSSL_LIBRARIES = ${OPENSSL_LIBRARIES}")

find_package(mysql-concpp REQUIRED COMPONENTS jdbc)
message(STATUS "MYSQL_CONCPP_INCLUDE_DIR = ${MYSQL_CONCPP_INCLUDE_DIR}")
if(TARGET mysql::concpp-jdbc)
    message(STATUS "Target mysql::concpp-jdbc exists")
elseif(TARGET mysql::concpp-jdbc-static)
    message(STATUS "Target mysql::concpp-jdbc-static exists")
else()
    message(WARNING "No mysql target found!")
endif()

# 统一使用存在的目标
update_cached_list(MYSQL_LIBRARIES mysql::concpp-jdbc)   # ← 修改此处

find_package(rabbitmq-c REQUIRED)
if(TARGET rabbitmq::rabbitmq-static)
    message(STATUS "Target rabbitmq::rabbitmq-static exists")
endif()
update_cached_list(RABBITMQ_LIBRARIES rabbitmq::rabbitmq-static)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdpart/msgpack)

add_subdirectory(Framework)


file(GLOB_RECURSE FRAMEWORK_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_library(Framework STATIC ${FRAMEWORK_SOURCES})

# 1. 将 Boost 包含目录设为 PUBLIC（因为 Framework 头文件可能包含 Boost）
target_include_directories(Framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}          # 正确
)

# 2. OpenSSL 的头文件目录也需要添加（除非你使用目标传递）
#    若使用 OpenSSL::SSL 目标，则不用手动添加
target_include_directories(Framework PRIVATE ${OPENSSL_INCLUDE_DIR})

# 3. 链接库：根据依赖是否暴露给外部决定 PUBLIC/PRIVATE
#    假设 Framework 的公共接口不直接使用这些库的类型，则 PRIVATE 即可
target_link_libraries(Framework PRIVATE
    ${OPENSSL_LIBRARIES}           # 若用目标则换成 OpenSSL::SSL OpenSSL::Crypto
    ${3RDPART_LIBRARIES}
    ${MYSQL_LIBRARIES}             # 目标
    ${RABBITMQ_LIBRARIES}          # 目标
)

target_include_directories(Framework PUBLIC ${NLOHMANN_JSON_INCLUDE_DIR})


# 4. 将 Framework 自身注册到全局缓存（供后续可执行文件使用）
update_cached_list(FRAMEWORK_LIBRARY Framework)
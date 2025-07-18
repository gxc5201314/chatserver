# 定义了一个SRC_LIST变量，包换了该目录下所有源文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_list)
aux_source_directory(./model MODEL_list)
aux_source_directory(./redis REDIS_list)
# 指定生产可执行文件
add_executable(ChatServer ${SRC_LIST} ${DB_list} ${MODEL_list} ${REDIS_list})
# 指定可执行文件链接时需要依赖的库文件
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread)
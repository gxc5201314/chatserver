# 配置头文件的搜索路径
# include_directories()
# 配置库文件的搜索路径
# link_directories()

# 设置需要编译的源文件列表、
set(SRC_LIST ./muduo_server.cpp)

# 设置可执行文件存储目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 把.指定路径下的所有源文件名字放入变量名SRC_LIST里面
# aux_source_directory(.SRC_LIST);

# 表示生产可执行文件server，由SRC_LIST变量所定义的源文件编译而来
add_executable(server ${SRC_LIST})
# 表示server这个目标程序，需要连接muduo_net muduo_base pthread这三个文件库
target_link_libraries(server muduo_net muduo_base pthread)

/**
 * Wingman Python Binding Tests
 * camelCase -> snake_case 名字转换单元测试
 *
 * 这里只测 wingman::python::camelToSnake 纯函数，不需要初始化 CPython 解释器，
 * 因此可以在任意构建目标（含 CI 上的无 Python 运行时节点）上独立运行。
 *
 * 该符号定义在 libs/python/src/python_script_engine.cpp 的 wingman::python 命名空间内，
 * 不在公共头文件中导出，因此这里通过 extern 声明引用。
 */

#include <gtest/gtest.h>
#include <string>

namespace wingman {
namespace python {
// 纯函数，定义见 python_script_engine.cpp。
// 不放入公共头文件以避免污染绑定层公开 API。
std::string camelToSnake(const std::string& camel);
} // namespace python
} // namespace wingman

using wingman::python::camelToSnake;

// ========== 任务给定的标准用例 ==========

TEST(CamelToSnakeTest, GetForeground) {
	EXPECT_EQ(camelToSnake("getForeground"), "get_foreground");
}

TEST(CamelToSnakeTest, MouseRightClickKeepsExistingUnderscore) {
	EXPECT_EQ(camelToSnake("mouse_rightClick"), "mouse_right_click");
}

TEST(CamelToSnakeTest, GetTime) {
	EXPECT_EQ(camelToSnake("getTime"), "get_time");
}

TEST(CamelToSnakeTest, AllLowercaseUnchanged) {
	EXPECT_EQ(camelToSnake("hexists"), "hexists");
}

TEST(CamelToSnakeTest, IsEmpty) {
	EXPECT_EQ(camelToSnake("isEmpty"), "is_empty");
}

TEST(CamelToSnakeTest, ConsecutiveUppercaseURL) {
	EXPECT_EQ(camelToSnake("parseURL"), "parse_url");
}

TEST(CamelToSnakeTest, TwoLettersUnchanged) {
	EXPECT_EQ(camelToSnake("ml"), "ml");
}

TEST(CamelToSnakeTest, SetHotReload) {
	EXPECT_EQ(camelToSnake("setHotReload"), "set_hot_reload");
}

// ========== 连续大写（首字母缩写词）补充用例 ==========

TEST(CamelToSnakeTest, ConsecutiveUppercaseHTTP) {
	EXPECT_EQ(camelToSnake("getHTTP"), "get_http");
}

TEST(CamelToSnakeTest, LeadingUppercaseAcronym) {
	EXPECT_EQ(camelToSnake("XMLParser"), "xml_parser");
}

TEST(CamelToSnakeTest, TrailingAcronym) {
	EXPECT_EQ(camelToSnake("fetchURL"), "fetch_url");
}

// ========== 边界用例 ==========

TEST(CamelToSnakeTest, EmptyString) {
	EXPECT_EQ(camelToSnake(""), "");
}

TEST(CamelToSnakeTest, SingleUppercaseLeading) {
	EXPECT_EQ(camelToSnake("Hello"), "hello");
}

TEST(CamelToSnakeTest, SingleCharacterLower) {
	EXPECT_EQ(camelToSnake("a"), "a");
}

TEST(CamelToSnakeTest, SingleCharacterUpper) {
	EXPECT_EQ(camelToSnake("A"), "a");
}

TEST(CamelToSnakeTest, AllLowercaseWord) {
	EXPECT_EQ(camelToSnake("already_snake"), "already_snake");
}

TEST(CamelToSnakeTest, AllUppercase) {
	EXPECT_EQ(camelToSnake("URL"), "url");
}

TEST(CamelToSnakeTest, AlreadySnakeCaseNoDoubleUnderscore) {
	EXPECT_EQ(camelToSnake("foo_bar"), "foo_bar");
}

TEST(CamelToSnakeTest, UnderscoreThenUpperNoDoubleUnderscore) {
	EXPECT_EQ(camelToSnake("_privateField"), "_private_field");
}

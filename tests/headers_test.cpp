#include "headers.h"
#include <gtest/gtest.h>

using namespace std::string_view_literals;
using NamVal = std::pair<std::string, std::string>;
using Headers = std::vector<NamVal>;

TEST(iterHeaders, Empty) {
    Headers check{};
    auto req = ""sv;

    auto callback_ = [&check](std::string_view nam, std::string_view val) {
        check.emplace_back(std::string(nam), std::string(val));
    };
    iterHeaders(req, callback_);

    EXPECT_TRUE(check.empty());
}

TEST(iterHeaders, SkipRequestLine) {
    Headers check{};
    auto req = "GET / HTTP/1.1\r\n\r\n\r\n"sv;

    auto callback_ = [&check](std::string_view nam, std::string_view val) {
        check.emplace_back(std::string(nam), std::string(val));
    };
    iterHeaders(req, callback_);

    EXPECT_TRUE(check.empty());
}

TEST(iterHeaders, SingleHeader) {
    Headers check{};
    auto req = "GET / HTTP/1.1\r\nHOST:\tya.ru\r\n\r\n"sv;

    auto callback_ = [&check](std::string_view nam, std::string_view val) {
        check.emplace_back(std::string(nam), std::string(val));
    };
    iterHeaders(req, callback_);

    EXPECT_FALSE(check.empty());
    EXPECT_EQ(check.size(), 1);
    EXPECT_EQ(check[0].first, "HOST");
    EXPECT_EQ(check[0].second, "ya.ru");
}

TEST(iterHeaders, MultipleHeaders) {
    Headers check{};
    auto req =
        "GET / HTTP/1.1\r\nHOST:    ya.ru\r\nContent-Length:\t200\r\nAccept:text/html\r\nAccept-Language: ru-RU,ru;q=0.9\r\n\r\n"sv;

    auto callback_ = [&check](std::string_view nam, std::string_view val) {
        check.emplace_back(std::string(nam), std::string(val));
    };
    iterHeaders(req, callback_);

    EXPECT_FALSE(check.empty());
    EXPECT_EQ(check.size(), 4);
    EXPECT_EQ(check[0].first, "HOST");
    EXPECT_EQ(check[0].second, "ya.ru");
    EXPECT_EQ(check[1].first, "Content-Length");
    EXPECT_EQ(check[1].second, "200");
    EXPECT_EQ(check[2].first, "Accept");
    EXPECT_EQ(check[2].second, "text/html");
    EXPECT_EQ(check[3].first, "Accept-Language");
    EXPECT_EQ(check[3].second, "ru-RU,ru;q=0.9");
}

TEST(iterHeaders, MultipleSameHeaders) {
    Headers check{};
    auto req = "GET / HTTP/1.1\r\nHOST:ya.ru\r\nHOST:ya.ru\r\nContent-Length:\t200\r\n\r\n"sv;

    auto callback_ = [&check](std::string_view nam, std::string_view val) {
        check.emplace_back(std::string(nam), std::string(val));
    };
    iterHeaders(req, callback_);

    EXPECT_FALSE(check.empty());
    EXPECT_EQ(check.size(), 3);
    EXPECT_EQ(check[0].first, "HOST");
    EXPECT_EQ(check[0].second, "ya.ru");
    EXPECT_EQ(check[1].first, "HOST");
    EXPECT_EQ(check[1].second, "ya.ru");
    EXPECT_EQ(check[2].first, "Content-Length");
    EXPECT_EQ(check[2].second, "200");
}

TEST(findHostPort, Simple) {
    auto req = "GET / HTTP/1.1\r\nHOST:\tya.ru:8080\r\n\r\n"sv;

    auto [host, port] = findHostPort(req);

    EXPECT_EQ(host, "ya.ru");
    EXPECT_EQ(port, "8080");
}

TEST(findHostPort, NoHost) {
    auto req = "GET / HTTP/1.1\r\nHOST:\r\n\r\n"sv;

    auto [host, port] = findHostPort(req);

    EXPECT_EQ(host, "");
    EXPECT_EQ(port, "80");
}

TEST(findHostPort, NoPort) {
    auto req = "GET / HTTP/1.1\r\nHOST:\tya.ru\r\n\r\n"sv;

    auto [host, port] = findHostPort(req);

    EXPECT_EQ(host, "ya.ru");
    EXPECT_EQ(port, "80");
}

TEST(findContentLength, Simple) {
    auto req = "GET / HTTP/1.1\r\nHOST:\tya.ru:8080\r\nContent-Length:\t200\r\n\r\n"sv;

    auto check = findContentLength(req);

    EXPECT_EQ(check, 200);
}

TEST(findContentLength, NoContentLength) {
    auto req = "GET / HTTP/1.1\r\nHOST:\tya.ru:8080\r\n\r\n"sv;

    auto check = findContentLength(req);

    EXPECT_TRUE(check.has_value() == false);
}

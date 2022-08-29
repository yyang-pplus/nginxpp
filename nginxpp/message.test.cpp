#include <nginxpp/message.hpp>

#include <gtest/gtest.h>

#include <nginxpp/exception.hpp>


using namespace nginxpp;


TEST(ParserTest, CanParseSampleHTTP) {
    std::istringstream ss {R"(GET /home.html HTTP/1.1
Host: developer.mozilla.org
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.9; rv:50.0) Gecko/20100101 Firefox/50.0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate, br
Referer: https://developer.mozilla.org/testpage.html
Connection: keep-alive
Upgrade-Insecure-Requests: 1
If-Modified-Since: Mon, 18 Jul 2016 02:36:04 GMT
If-None-Match: "c561c68d0ba92bbeb8b0fff2a9199f722e3a621a"
Cache-Control: max-age=0

)"};

    const auto a_request = ParseOne(ss);

    EXPECT_EQ(Method::GET, a_request.method);
    EXPECT_EQ("home.html", a_request.target);
    EXPECT_EQ("HTTP/1.1", a_request.version);
    ASSERT_EQ(11, a_request.headers.size());
}

TEST(ParserTest, CanParseChromeSampleHTTP) {
    std::istringstream ss {R"(GET / HTTP/1.1
Host: localhost:19840
Connection: keep-alive
Cache-Control: max-age=0
sec-ch-ua: " Not A;Brand";v="99", "Chromium";v="101", "Google Chrome";v="101"
sec-ch-ua-mobile: ?0
sec-ch-ua-platform: "Linux"
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/101.0.0.0 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
Sec-Fetch-Site: none
Sec-Fetch-Mode: navigate
Sec-Fetch-User: ?1
Sec-Fetch-Dest: document
Accept-Encoding: gzip, deflate, br
Accept-Language: zh-CN,zh;q=0.9,en-US;q=0.8,en;q=0.7

)"};

    const auto a_request = ParseOne(ss);

    EXPECT_EQ(Method::GET, a_request.method);
    EXPECT_EQ("", a_request.target);
    EXPECT_EQ("HTTP/1.1", a_request.version);
    ASSERT_EQ(15, a_request.headers.size());
}

TEST(ParserTest, ErrorIfMissingStartLine) {
    std::istringstream ss;

    const auto a_request = ParseOne(ss);
    EXPECT_NE(200, a_request.status);
    EXPECT_FALSE(a_request);
}

TEST(ParserTest, ErrorIfInvalidStartLine) {
    std::istringstream ss {R"(GET HTTP/1.1)"};

    const auto a_request = ParseOne(ss);
    EXPECT_NE(200, a_request.status);
    EXPECT_FALSE(a_request);
}

TEST(ParserTest, ErrorIfVersionNotSupported) {
    std::istringstream ss {R"(GET / HTTP/2.1)"};

    const auto a_request = ParseOne(ss);
    EXPECT_NE(200, a_request.status);
    EXPECT_FALSE(a_request);
}

TEST(ParserTest, ErrorIfUnknownMethod) {
    std::istringstream ss {R"(PICK /home.html HTTP/1.1)"};

    const auto a_request = ParseOne(ss);
    EXPECT_NE(200, a_request.status);
    EXPECT_FALSE(a_request);
}

TEST(ParserTest, ErrorIfMethodNotImplemented) {
    std::istringstream ss {R"(TRACE /home.html HTTP/1.1)"};

    const auto a_request = ParseOne(ss);
    EXPECT_NE(200, a_request.status);
    EXPECT_FALSE(a_request);
}

TEST(ParserTest, ErrorIfTargetTooLong) {
    std::istringstream ss {"TRACE " + std::string(MAX_LINE_LENGTH, '/') + "/home.html HTTP/1.1"};

    const auto a_request = ParseOne(ss);
    EXPECT_NE(200, a_request.status);
    EXPECT_FALSE(a_request);
}

TEST(ParserTest, CanParseHeaders) {
    std::istringstream ss {R"(GET /home.html HTTP/1.1
Connection: keep-alive
Upgrade-Insecure-Requests: 1
)"};

    const auto a_request = ParseOne(ss);

    EXPECT_EQ(Method::GET, a_request.method);
    EXPECT_EQ("home.html", a_request.target);
    EXPECT_EQ("HTTP/1.1", a_request.version);

    ASSERT_EQ(2, a_request.headers.size());
    EXPECT_EQ("keep-alive", a_request.headers.at("connection"));
    EXPECT_EQ("1", a_request.headers.at("upgrade-insecure-requests"));
}

TEST(ParserTest, DuplicateHeadersShallBeCombined) {
    std::istringstream ss {R"(GET /home.html HTTP/1.1
Sample: one
Sample: 1
)"};

    const auto a_request = ParseOne(ss);

    EXPECT_EQ(Method::GET, a_request.method);
    EXPECT_EQ("home.html", a_request.target);
    EXPECT_EQ("HTTP/1.1", a_request.version);

    ASSERT_EQ(1, a_request.headers.size());
    EXPECT_EQ("one, 1", a_request.headers.at("sample"));
}

TEST(ParserTest, BreakAtBlankLine) {
    std::istringstream ss {R"(GET /home.html HTTP/1.1
Connection: keep-alive
Upgrade-Insecure-Requests: 1

Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate, br
Referer: https://developer.mozilla.org/testpage.html
)"};

    const auto a_request = ParseOne(ss);

    EXPECT_EQ(Method::GET, a_request.method);
    EXPECT_EQ("home.html", a_request.target);
    EXPECT_EQ("HTTP/1.1", a_request.version);

    ASSERT_EQ(2, a_request.headers.size());
    EXPECT_EQ("keep-alive", a_request.headers.at("connection"));
    EXPECT_EQ("1", a_request.headers.at("upgrade-insecure-requests"));
}

TEST(ParserTest, NoThrowIfInvalidHeader) {
    std::istringstream ss {R"(GET /home.html HTTP/1.1
Host:
Connection:keep-alive
Upgrade-Insecure-Requests: 1
max-age=0
)"};

    Request a_request;
    ASSERT_NO_THROW(a_request = ParseOne(ss));

    EXPECT_EQ(Method::GET, a_request.method);
    EXPECT_EQ("home.html", a_request.target);
    EXPECT_EQ("HTTP/1.1", a_request.version);

    ASSERT_EQ(2, a_request.headers.size());
    EXPECT_EQ("keep-alive", a_request.headers.at("connection"));
    EXPECT_EQ("1", a_request.headers.at("upgrade-insecure-requests"));
}

TEST(ParserTest, ErrorIfHeaderTooLong) {
    std::istringstream ss {R"(GET /home.html HTTP/1.1
Connection: keep-alive
Long-Header:)" + std::string(MAX_LINE_LENGTH + 1, '*') +
                           '\n'};

    const auto a_request = ParseOne(ss);

    EXPECT_NE(200, a_request.status);
    EXPECT_FALSE(a_request);
}


TEST(HandleTest, ErrorIfInvalidRequest) {
    Request a_request;
    a_request.status = 500;
    a_request.error_str = "Error";

    const auto a_response = Handle(a_request, std::filesystem::current_path());
    EXPECT_FALSE(a_response);
    EXPECT_EQ(a_request.error_str, a_response.error_str);
    EXPECT_EQ(a_request.status, a_response.status);
}

TEST(HandleTest, ErrorIfRequestFileOutSideRoot) {
    Request a_request;
    a_request.target = "..";

    const auto a_response = Handle(a_request, std::filesystem::current_path());
    EXPECT_FALSE(a_response);
}

TEST(HandleTest, ErrorIfRequestFileNotExist) {
    Request a_request;
    a_request.target = "no_such_file";

    const auto a_response = Handle(a_request, std::filesystem::current_path());
    EXPECT_FALSE(a_response);
}

TEST(HandleTest, HasBodyIfRequestDir) {
    Request a_request;
    a_request.target = ".";

    const auto a_response = Handle(a_request, std::filesystem::current_path());
    ASSERT_TRUE(a_response);
    EXPECT_TRUE(a_response.body_stream);
}

TEST(HandleTest, HasBodyIfRequestFile) {
    Request a_request;
    a_request.target = "Makefile";

    const auto a_response = Handle(a_request, std::filesystem::current_path());
    ASSERT_TRUE(a_response);
    EXPECT_TRUE(a_response.body_stream);
}

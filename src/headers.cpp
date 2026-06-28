#include "../include/headers.h"

#include <algorithm>
#include <ranges>
#include <string_view>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

// helpers
auto to_lower = [](unsigned char c) { return std::tolower(c); };
auto is_digit = [](unsigned char c) { return std::isdigit(c); };

void iterHeaders(std::string_view req, Callback &&callback) {
    // разделяем string_view на строки
    std::ranges::split_view req_strs = req | std::views::split("\r\n"sv);

    // если строк нет, выходим
    if (req_strs.begin() == req_strs.end()) {
        return;
    }

    // получаем итератор на первую строку
    auto it = req_strs.begin();
    // и пропускаем ее (request line)
    ++it;

    while (it != req_strs.end()) {
        // получаем строку как string_view
        std::string_view str(std::ranges::data(*it), std::ranges::size(*it));
        if (str.size() > 0) {
            size_t pos = str.find(':');
            if (pos != std::string_view::npos) {
                // разделяем строку на подстроки имя и значение
                std::string_view name = str.substr(0, pos);
                std::string_view val = str.substr(pos + 1, str.size() - pos - 1);

                // отсекаем табуляцию и пробелы в начале значения
                size_t val_begin = val.find_first_not_of(" \t");

                if (val_begin == std::string_view::npos) {
                    callback(name, "");
                } else {
                    val = val.substr(val_begin);
                    callback(name, val);
                }
            }
        }
        ++it;
    }
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    // возвращаем пару <host, port>
    // по умолчанию порт = 80
    std::pair<std::string, std::string> ret{{}, "80"};

    auto callback_ = [&ret](std::string_view name, std::string_view val) {
        // приводим имя заголовка к нижнему регистру (lnam)
        std::string lnam{};
        std::ranges::transform(name, std::back_inserter(lnam), to_lower);

        if (lnam == "host") {
            // двоеточие как разделитель между хостом и портом
            size_t colon_pos = val.find(':');
            if (colon_pos != std::string_view::npos) {
                // порт после ':'
                std::string_view host_substr = val.substr(0, colon_pos);
                std::string_view port_substr = val.substr(colon_pos + 1, val.size() - colon_pos - 1);

                size_t port{0};
                // используем std::from_chars для проверки на валидность порта э [0, 65535]
                auto [_, ec] = std::from_chars(port_substr.data(), port_substr.data() + port_substr.size(), port);
                if (ec == std::errc{} && port > 0 && port <= 65535) {
                    ret.second = port_substr;
                }
                ret.first = host_substr;
            } else {
                // если двоеточия нет, сохраняем хост как полную строку
                ret.first = val;
            }
        }
    };

    iterHeaders(req, callback_);
    return ret;
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    // возвращаем std::nullopt, если заголовок отсутствует
    std::optional<size_t> ret = std::nullopt;

    auto callback_ = [&ret](std::string_view name, std::string_view val) {
        // приводим имя заголовка к нижнему регистру (lnam)
        std::string lnam{};
        std::ranges::transform(name, std::back_inserter(lnam), to_lower);

        if (lnam == "content-length") {
            size_t len{0};
            // используем std::from_chars для безопасного преобразования строки в число
            auto [_, ec] = std::from_chars(val.data(), val.data() + val.size(), len);
            if (ec == std::errc{}) {
                ret = len;
            }
        }
    };

    iterHeaders(rsp, callback_);
    return ret;
}

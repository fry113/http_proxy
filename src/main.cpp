// для починки boost::asio::awaitable и пр.
#define BOOST_ASIO_HAS_CO_RETURNS 1

#include "../include/headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/version.hpp>
#include <print>

#include <iostream>
#include <string_view>

using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::dynamic_buffer;
using boost::asio::io_service;
using boost::asio::transfer_at_least;
using boost::asio::use_awaitable;
using boost::asio::ip::basic_resolver_results;
using boost::asio::ip::tcp;
using boost::system::error_code;

constexpr std::string_view delimiter = "\r\n\r\n";
inline constexpr size_t DEFAULT_CHUNK_SIZE = 0x2000;  // 8192 байт

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {
    try {
        // получаем запрос от клиента
        std::string request;
        co_await async_read_until(client_socket, dynamic_buffer(request), delimiter, use_awaitable);

        // ищем хост и номер порта в заголовках (порт = 80 по умолчанию)
        auto [host, port] = findHostPort(request);
        if (host.size() == 0) {
            std::cerr << "No host found";
            client_socket.close();
            co_return;
        }

        // резольвим хост и порт
        tcp::resolver resolver(io_service);
        basic_resolver_results<tcp> connection = co_await resolver.async_resolve(host, port, use_awaitable);

        // создаем сокет
        tcp::socket server_socket(io_service);
        co_await boost::asio::async_connect(server_socket, connection, use_awaitable);

        // отправляем запрос на сервер
        co_await async_write(server_socket, buffer(request), use_awaitable);

        // читаем ответ от сервера
        std::string response;
        co_await async_read_until(server_socket, dynamic_buffer(response), delimiter, use_awaitable);
        std::println("Connection established at {}:{}", host, port);

        // отправляем заголовки клиенту
        std::optional<size_t> content_length = findContentLength(response);
        co_await async_write(client_socket, buffer(response), use_awaitable);

        // если заголовок отсутствует, читаем до конца потока
        std::string body;
        if (!content_length.has_value()) {
            while (true) {
                boost::system::error_code boost_error;            // для обработки EOF и других ошибок async_read
                co_await async_read(server_socket,                // сокет сервера
                                    dynamic_buffer(body),         // динамический буфер для чтения
                                    transfer_at_least(1),         // сколько пришло, столько читаем
                                    boost::asio::redirect_error(  // перенаправляем ошибки в boost_error
                                        use_awaitable,            // используем use_awaitable
                                        boost_error               // для EOF и других ошибок
                                        ));

                // если данные есть, отправляем их клиенту
                if (body.size() > 0) {
                    co_await async_write(client_socket, buffer(body), use_awaitable);
                }
                // очищаем буфер для следующей итерации
                body.clear();

                if (boost_error) {
                    // если EOF - выходим из цикла
                    if (boost_error == boost::asio::error::eof) {
                        break;
                    }
                    // если другая ошибка - бросаем исключение
                    std::cerr << "Сетевой сбой boost_error: " << boost_error.message() << std::endl;
                    throw std::runtime_error(boost_error.message());
                }
            }
        } else {
            // размер тела = полный размер данных - (размер ответа - начало тела)
            size_t body_size =
                content_length.value() - (response.size() - (response.find(delimiter) + delimiter.size()));

            std::string body_chunk;
            body_chunk.resize(DEFAULT_CHUNK_SIZE);
            while (body_size > 0) {
                size_t n = co_await async_read(
                    server_socket,                                    // сокет сервера
                    buffer(body_chunk.data(),                         // буфер для чтения
                           std::min(body_size, DEFAULT_CHUNK_SIZE)),  // читаем не больше DEFAULT_CHUNK_SIZE за раз
                    transfer_at_least(1),                             // но хотя бы 1 байт
                    use_awaitable                                     // используем use_awaitable
                );
                // очищаем буфер для следующей итерации
                body_chunk.clear();

                // отправляем прочитанные данные клиенту
                co_await async_write(client_socket, buffer(body_chunk.data(), n), use_awaitable);
                body_size -= n;
            }
        }

        // закрываем сокеты
        client_socket.close();
        server_socket.close();
        std::println("Connection closed at {}:{}", host, port);
    } catch (const std::exception &e) {
        std::cerr << "Session error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Session error: неизвестная ошибка" << std::endl;
    }
}

class Server {
public:  // public methods
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), socket_(io_service) {
        do_accept();
    }

private:  // private methods
    void do_accept() {
        acceptor_.async_accept(socket_, [this](error_code ec) {
            if (!ec) {
                co_spawn(io_service_, session(std::move(socket_), io_service_), boost::asio::detached);
            }
            socket_ = tcp::socket(io_service_);
            do_accept();
        });
    }

private:  // private vars
    io_service &io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: proxy_server";
            std::cerr << " <listen_port>\n";
            return 1;
        }
        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

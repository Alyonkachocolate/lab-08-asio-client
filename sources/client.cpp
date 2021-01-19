// Copyright [2021] <Alyona Dorodnyaya>

#include <client.hpp>
#include <utility>

using boost::asio::buffer;
using boost::asio::ip::tcp;

boost::asio::io_service service;

// класс для клиента
class talk_to_server {
 private:
  tcp::socket socket_;
  int already_read_;
  char buff_[1024];
  bool started_;  // меняется в сервере
  std::string username_;

 public:
  explicit talk_to_server(std::string username)
      : socket_(service), started_(true), username_(std::move(username)) {}

  // коннект с сервером
  void connect(tcp::endpoint ep) { socket_.connect(ep); }

  // запрос клиента
  void request() {
    socket_.write_some(buffer("LOGIN: " + username_ + "\n"));
    read_answer();
    // пока не отключили, запрос -> ответ -> засыпаем (максимум на 7с)
    while (started_) {
      write_request();
      read_answer();
      boost::this_thread::sleep(boost::posix_time::millisec(rand_r(0) % 7000));
    }
  }

  // читаем ответ
  void read_answer() {
    already_read_ = 0;
    read(socket_, buffer(buff_),
         boost::bind(&talk_to_server::read_complete, this, _1, _2));
    process_msg();
  }

  // проверка на наличие символа "\n" и ошибки
  size_t read_complete(const boost::system::error_code& err, size_t bytes) {
    if (err) return 0;
    bool found = std::find(buff_, buff_ + bytes, '\n') < buff_ + bytes;
    return found ? 1 : 0;
  }

  void process_msg() {
    std::string msg(buff_, already_read_);
    // при заходе на сервер просим ввести запрос
    if (msg.find("Login ") == 0) on_login();
    // ping - "ping_OK" или "client_list_changed"
    else if (msg.find("Ping") == 0)
      on_ping(msg);
    // выводим список клиентов
    else if (msg.find("Clients") == 0)
      on_clients(msg);
    else
      std::cerr << "Invalid msg " << msg << std::endl;
  }

  void do_command() {
    socket_.write_some(buffer("Write a request \n"));
    read_answer();
  }

  void on_login() { do_command(); }

  void on_ping(const std::string& msg) {
    std::istringstream in(msg);
    std::string answer;
    in >> answer >> answer;
    if (answer == "client_list_changed") do_command();
  }

  static void on_clients(const std::string& msg) {
    std::string clients = msg.substr(8);
    std::cout << "New client list: " << clients;
  }

  // знак, что клиент не отключен
  void write_request() { socket_.write_some(buffer("PING\n")); }
};

// функция для подключения к серверу
boost::asio::ip::tcp::endpoint ep(tcp::v4(), 8001);
void run_client(const std::string& client_name) {
  talk_to_server client(client_name);
  try {
    client.connect(ep);
    client.request();
  } catch (boost::system::system_error& err) {
    std::cout << "Client terminated" << std::endl;
  }
}

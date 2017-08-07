#ifdef WIN32
#define _WIN32_WINNT 0x0501
#endif // WIN32

#include "server.h"
#include <QMessageBox>
//#include <iostream>
#include <set>
#include <queue>
#include <thread>
#include <cstdlib>
#include <boost/asio.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "../Include/chat_message.h"

Server::Server(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	//ui.lineEdit->setText(QString::fromLocal8Bit("你好世界"));
	ui.lineEdit->setText("8888");
	connect(ui.pushButton, SIGNAL(clicked()), this, SLOT(slotStartServer()));

}

Server::~Server()
{

}

using namespace boost::asio;
using namespace boost::system;
using boost::asio::ip::tcp;

class chat_participant   //提供接口的方法类
{
public:
	virtual ~chat_participant() {}  //虚析构
	virtual void deliver(const chat_message& msg) = 0; //(纯虚函数)
};

//定义一个消息队列
typedef  std::deque<chat_message>  chat_message_queue;

class chat_room  //管理用户(平起平坐)
{
private:
	std::set<boost::shared_ptr<chat_participant> > participants_;  //存储用户的容器
	enum { max_recent_msgs = 100 };  //发送用户100条信息
	chat_message_queue recent_msgs_;  //历史聊天记录(没有大小限制)
public:
	void join_room(boost::shared_ptr<chat_participant> participant)
	{
		//std::cout << "有用户【加入】聊天室" << std::endl;

		participants_.insert(participant);  //加入到用户管理
											//给新加入的用户发送历史信息
		std::for_each(recent_msgs_.begin(), recent_msgs_.end(),
			boost::bind(&chat_participant::deliver, participant, _1)
		);
	}

	void leave(boost::shared_ptr<chat_participant> participant)
	{
		//std::cout << "有用户【离开】聊天室" << std::endl;
		participants_.erase(participant);
	}

	void deliver(const chat_message& msg)  //派发函数
	{
		recent_msgs_.push_back(msg);   //把消息压到消息队列里

		while (recent_msgs_.size() > max_recent_msgs)
		{
			recent_msgs_.pop_front();  //room里的聊天内容是有限的,多了就从头部弹出去（把最早的那些消息给扔了）
		}

		std::for_each(participants_.begin(), participants_.end(),
			boost::bind(&chat_participant::deliver, _1, boost::ref(msg))   //_1是for_each出来的迭代器participant
		);

	}
};


class chat_session : public chat_participant,  //<----有个纯虚方法
	public boost::enable_shared_from_this<chat_session>
{
private:
	tcp::socket socket_;
	chat_message read_msg_;
	chat_room&  room_;
	chat_message_queue write_msgs_;   //为个人维护一个聊天消息队列
public:
	chat_session(io_service& ioservice, chat_room& room)
		:socket_(ioservice)
		, room_(room)
	{

	}

	~chat_session()
	{

	}

	tcp::socket& socket()
	{
		return socket_;
	}

	void start()
	{
		room_.join_room(shared_from_this());  //加入聊天室

		boost::asio::async_read(socket_,      //接收内容
			boost::asio::buffer(read_msg_.data(), chat_message::header_length_),
			bind(&chat_session::read_handle_header, shared_from_this(), _1)
		);
	}

	void read_handle_header(const error_code& error)
	{
		if (!error && read_msg_.decode_header())
		{
			boost::asio::async_read(socket_,
				boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
				bind(&chat_session::read_handle_body, shared_from_this(), _1)
			);
		}
		else
		{
			//踢出去
			room_.leave(shared_from_this());
		}
	}

	void read_handle_body(const error_code& error)
	{
		if (!error)
		{
// 			std::string str(read_msg_.body(), read_msg_.body_length());
// 
// 			std::cout << "内容为:" << str
// 				<< " 内容长度:" << read_msg_.body_length() << std::endl;

			room_.deliver(read_msg_);  //直接投递给聊天室(消息发送给多个人)

			boost::asio::async_read(socket_,
				boost::asio::buffer(read_msg_.data(), chat_message::header_length_),
				bind(&chat_session::read_handle_header, shared_from_this(), _1)
			);
		}
		else
		{
			room_.leave(shared_from_this());
		}
	}

	virtual void deliver(const chat_message& msg)
	{
		bool write_in_progress = !write_msgs_.empty();

		write_msgs_.push_back(msg);   //把要发送的内容全都压倒为某个对象服务的队列容器中

		if (!write_in_progress)
		{
			boost::asio::async_write(socket_,
				buffer(write_msgs_.front().data(), write_msgs_.front().length()),
				boost::bind(&chat_session::write_handle, shared_from_this(), _1));
		}
	}

	void write_handle(const error_code& error)
	{
		if (!error)
		{
			write_msgs_.pop_front();  //扔掉一个

			if (!write_msgs_.empty())
			{
				boost::asio::async_write(socket_,
					buffer(write_msgs_.front().data(), write_msgs_.front().length()),
					boost::bind(&chat_session::write_handle, shared_from_this(), _1));
			}
		}
		else
		{
			room_.leave(shared_from_this());
		}
	}
};

class chat_server
{
private:
	io_service& io_service_;
	tcp::acceptor acceptor_;
	chat_room   room_;  //生成聊天室
public:
	chat_server(io_service& ioservice, tcp::endpoint endpoint)
		:io_service_(ioservice),
		acceptor_(ioservice, endpoint)
	{
		start_accept();
	}

	void start_accept()
	{
		boost::shared_ptr<chat_session> new_session(boost::make_shared<chat_session>(io_service_, room_));  //生成新的会话

		acceptor_.async_accept(new_session->socket(), boost::bind(&chat_server::accept_handle, this, new_session, _1));

	}

	void accept_handle(boost::shared_ptr<chat_session> session, const error_code& error)
	{
		if (!error)
		{
// 			std::cout << "IP地址:" << session->socket().remote_endpoint().address()
// 				<< " 端口号:" << session->socket().remote_endpoint().port() << std::endl;

			session->start();
		}

		start_accept();
	}
};

void Server::slotStartServer()
{
// 	QMessageBox::critical(NULL, QString::fromLocal8Bit("标题"),
// 		ui.lineEdit->text(),
// 		QMessageBox::Ok);

	std::thread t(
		[&]() 
		{
			io_service ioservice;

			tcp::endpoint endpoint(tcp::v4(), ui.lineEdit->text().toInt());

			chat_server server(ioservice, endpoint);

			ioservice.run();
		}
	);

	t.detach();
}

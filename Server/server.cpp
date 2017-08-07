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
	//ui.lineEdit->setText(QString::fromLocal8Bit("�������"));
	ui.lineEdit->setText("8888");
	connect(ui.pushButton, SIGNAL(clicked()), this, SLOT(slotStartServer()));

}

Server::~Server()
{

}

using namespace boost::asio;
using namespace boost::system;
using boost::asio::ip::tcp;

class chat_participant   //�ṩ�ӿڵķ�����
{
public:
	virtual ~chat_participant() {}  //������
	virtual void deliver(const chat_message& msg) = 0; //(���麯��)
};

//����һ����Ϣ����
typedef  std::deque<chat_message>  chat_message_queue;

class chat_room  //�����û�(ƽ��ƽ��)
{
private:
	std::set<boost::shared_ptr<chat_participant> > participants_;  //�洢�û�������
	enum { max_recent_msgs = 100 };  //�����û�100����Ϣ
	chat_message_queue recent_msgs_;  //��ʷ�����¼(û�д�С����)
public:
	void join_room(boost::shared_ptr<chat_participant> participant)
	{
		//std::cout << "���û������롿������" << std::endl;

		participants_.insert(participant);  //���뵽�û�����
											//���¼�����û�������ʷ��Ϣ
		std::for_each(recent_msgs_.begin(), recent_msgs_.end(),
			boost::bind(&chat_participant::deliver, participant, _1)
		);
	}

	void leave(boost::shared_ptr<chat_participant> participant)
	{
		//std::cout << "���û����뿪��������" << std::endl;
		participants_.erase(participant);
	}

	void deliver(const chat_message& msg)  //�ɷ�����
	{
		recent_msgs_.push_back(msg);   //����Ϣѹ����Ϣ������

		while (recent_msgs_.size() > max_recent_msgs)
		{
			recent_msgs_.pop_front();  //room����������������޵�,���˾ʹ�ͷ������ȥ�����������Щ��Ϣ�����ˣ�
		}

		std::for_each(participants_.begin(), participants_.end(),
			boost::bind(&chat_participant::deliver, _1, boost::ref(msg))   //_1��for_each�����ĵ�����participant
		);

	}
};


class chat_session : public chat_participant,  //<----�и����鷽��
	public boost::enable_shared_from_this<chat_session>
{
private:
	tcp::socket socket_;
	chat_message read_msg_;
	chat_room&  room_;
	chat_message_queue write_msgs_;   //Ϊ����ά��һ��������Ϣ����
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
		room_.join_room(shared_from_this());  //����������

		boost::asio::async_read(socket_,      //��������
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
			//�߳�ȥ
			room_.leave(shared_from_this());
		}
	}

	void read_handle_body(const error_code& error)
	{
		if (!error)
		{
// 			std::string str(read_msg_.body(), read_msg_.body_length());
// 
// 			std::cout << "����Ϊ:" << str
// 				<< " ���ݳ���:" << read_msg_.body_length() << std::endl;

			room_.deliver(read_msg_);  //ֱ��Ͷ�ݸ�������(��Ϣ���͸������)

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

		write_msgs_.push_back(msg);   //��Ҫ���͵�����ȫ��ѹ��Ϊĳ���������Ķ���������

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
			write_msgs_.pop_front();  //�ӵ�һ��

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
	chat_room   room_;  //����������
public:
	chat_server(io_service& ioservice, tcp::endpoint endpoint)
		:io_service_(ioservice),
		acceptor_(ioservice, endpoint)
	{
		start_accept();
	}

	void start_accept()
	{
		boost::shared_ptr<chat_session> new_session(boost::make_shared<chat_session>(io_service_, room_));  //�����µĻỰ

		acceptor_.async_accept(new_session->socket(), boost::bind(&chat_server::accept_handle, this, new_session, _1));

	}

	void accept_handle(boost::shared_ptr<chat_session> session, const error_code& error)
	{
		if (!error)
		{
// 			std::cout << "IP��ַ:" << session->socket().remote_endpoint().address()
// 				<< " �˿ں�:" << session->socket().remote_endpoint().port() << std::endl;

			session->start();
		}

		start_accept();
	}
};

void Server::slotStartServer()
{
// 	QMessageBox::critical(NULL, QString::fromLocal8Bit("����"),
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

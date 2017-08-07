#pragma once
#ifdef WIN32
#define _WIN32_WINNT 0x0501
#endif // WIN32

#include <queue>
#include <boost/asio.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include "../Include/chat_message.h"
#include <QObject>

using namespace boost::asio;
using namespace boost::system;
using boost::asio::ip::tcp;

//����һ����Ϣ����
typedef  std::deque<chat_message>  chat_message_queue;

class chat_client : public QObject
{
	Q_OBJECT  //(*)

    //�ź�
	Q_SIGNAL void received_packet(); //����Ϳ�����,�󶨵����齻��UI
private:
	io_service& io_service_;
	tcp::socket socket_;
	chat_message read_msg_;

	chat_message_queue write_msgs_;

	std::vector<chat_message> recv_msgs_;
public:
	chat_client(io_service& ioservice, tcp::resolver::iterator endpoint_iterator)
		:io_service_(ioservice)
		, socket_(ioservice)
	{
		boost::asio::async_connect(socket_, endpoint_iterator,
			bind(&chat_client::connect_handle, this, _1));
	}

	void connect_handle(const error_code& error)
	{
		if (!error)
		{
			boost::asio::async_read(socket_,
				boost::asio::buffer(read_msg_.data(), chat_message::header_length_),
				boost::bind(&chat_client::read_handle_header, this, _1)

			);
		}
	}

	void read_handle_header(const error_code& error)
	{
		if (!error && read_msg_.decode_header())
		{
			boost::asio::async_read(socket_,
				boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
				boost::bind(&chat_client::read_handle_body, this, _1)

			);
		}
		else
		{
			do_close();
		}
	}

	void handle_message(void)
	{
		recv_msgs_.push_back(read_msg_);
		emit received_packet(); //��������ź�
	}
	chat_message get_message()
	{
		chat_message m = recv_msgs_[0];
		recv_msgs_.erase(recv_msgs_.begin());
		return m;
	}

	void read_handle_body(const error_code& error)
	{
		if (!error)
		{
			// std::cout.write(read_msg_.body(), read_msg_.body_length());  //��������
			// std::cout << "\n";
			//QT<------
			handle_message();

			boost::asio::async_read(socket_,
				boost::asio::buffer(read_msg_.data(), chat_message::header_length_),
				boost::bind(&chat_client::read_handle_header, this, _1)

			);
		}
		else
		{
			do_close();
		}
	}

	void send(const chat_message& msg)
	{
		//ͬ��
		io_service_.post(boost::bind(&chat_client::do_write, this, msg));  //�첽��
	}

	void close()
	{
		io_service_.post(boost::bind(&chat_client::do_close, this));  //����ϢͶ�ݸ�ϵͳ(��������һ�ַ�ʽ),���ٶ�Ҳ����̫��
																	  //�൱���д���ĳ����PostMessage����
	}

	void do_write(chat_message msg)   //�������ݶ�ʧ
	{
		write_msgs_.push_back(msg);
		bool write_in_progress = !write_msgs_.empty();

		if (write_in_progress)
		{
			boost::asio::async_write(socket_,
				boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
				boost::bind(&chat_client::write_handle, this, _1)
			);
		}
	}

	void write_handle(const error_code& error)
	{
		if (!error)
		{
			write_msgs_.pop_front();

			if (!write_msgs_.empty())
			{
				boost::asio::async_write(socket_,
					boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
					boost::bind(&chat_client::write_handle, this, _1)
				);
			}
		}
		else
		{
			do_close();
		}
	}

	void do_close()
	{
		socket_.close();
	}
};

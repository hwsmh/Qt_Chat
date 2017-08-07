#ifndef CLIENT_H
#define CLIENT_H

#include <QtWidgets/QMainWindow>
#include "ui_client.h"
#include "chat_client.hpp"


class Client : public QMainWindow
{
	Q_OBJECT

public:
	Client(QWidget *parent = 0);
	~Client();

private:
	Ui::ClientClass ui;

	io_service ioservice_;

	boost::shared_ptr<chat_client> client_;
	boost::shared_ptr<io_service::work> work_;

	chat_message msg_;

private slots:
	void soltConnectServer();
	void soltSendMessage();
	void closeEvent(QCloseEvent *event);
	void handle_packet();
};

#endif // CLIENT_H

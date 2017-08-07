#ifndef SERVER_H
#define SERVER_H

#include <QtWidgets/QMainWindow>
#include "ui_server.h"

class Server : public QMainWindow
{
	Q_OBJECT

public:
	Server(QWidget *parent = 0);
	~Server();

private:
	Ui::ServerClass ui;

private slots:
	void slotStartServer();
};

#endif // SERVER_H

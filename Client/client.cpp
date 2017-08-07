#ifdef WIN32
#define _WIN32_WINNT 0x0501
#endif // WIN32

#include "client.h"
#include <QMessageBox>
#include <QCloseEvent>
#include <QDebug>
#include <QString>

Client::Client(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	ui.lineEdit->setText("127.0.0.1");
	ui.lineEdit_2->setText("8888");

	connect(ui.pushButton, SIGNAL(clicked()), this, SLOT(soltConnectServer()));
	connect(ui.pushButton_2, SIGNAL(clicked()), this, SLOT(soltSendMessage()));
}

Client::~Client()
{

}

void Client::closeEvent(QCloseEvent *event)
{
	qDebug() << QString::fromLocal8Bit("Debug:准备关闭");

	QMessageBox::StandardButton button;
	button = QMessageBox::question(this, QString::fromLocal8Bit("退出程序"),
		QString::fromLocal8Bit("是否结束操作退出?"),
		QMessageBox::Yes | QMessageBox::No);
	
	if (button == QMessageBox::No) 
	{
		event->ignore();  //忽略退出信号，程序继续运行
	}
	else if (button == QMessageBox::Yes)
	{
		if (client_.get() != nullptr)
		{
			client_->close();
			ioservice_.stop();
		}
		event->accept();  //接受退出信号，程序退出
	}
}

void Client::soltConnectServer()
{
// 	if (client_.get() != nullptr)
// 	{
// 		return;
// 	}
	tcp::resolver resolver(ioservice_);  //<----装配了一个解析器
	tcp::resolver::query query(ui.lineEdit->text().toStdString().c_str(), 
		                       ui.lineEdit_2->text().toStdString().c_str());  //多个IP（DNS）
	tcp::resolver::iterator iterator = resolver.resolve(query);

	client_ = boost::make_shared<chat_client>(ioservice_, iterator);

	work_ = boost::make_shared<io_service::work>(ioservice_);  //<-------没有工作io_service也不会停止工作

	//QT5的方法
	QObject::connect(client_.get(), SIGNAL(received_packet()), this, SLOT(handle_packet())); //<---捆绑两个类中的函数为一个消息响应

	boost::thread t(boost::bind(&boost::asio::io_service::run, &ioservice_));

	t.detach();
}

void Client::soltSendMessage()
{
	if (client_.get() != nullptr)
	{
		//QByteArray buf = ui.lineEdit_3->text().toLocal8Bit();
		QByteArray buf = ui.textEdit_2->toPlainText().toLocal8Bit();

		msg_.set_body_length(buf.size());  //设置包长
		memcpy(msg_.body(), buf.data(), buf.size());

		qDebug() << QString::fromLocal8Bit("Debug:发送内容:") << QString::fromLocal8Bit(buf.data())
			<< QString::fromLocal8Bit("发送长度:") << msg_.body_length();
		msg_.encode_header();

		client_->send(msg_);
	}
}

void Client::handle_packet()
{
	chat_message msg = client_->get_message();
	char szText[chat_message::max_body_length_ + 1] = { 0 };

	memcpy(szText, msg.body(), msg.body_length());

	ui.textEdit->append(QString::fromLocal8Bit(szText));

}
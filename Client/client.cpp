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
	qDebug() << QString::fromLocal8Bit("Debug:׼���ر�");

	QMessageBox::StandardButton button;
	button = QMessageBox::question(this, QString::fromLocal8Bit("�˳�����"),
		QString::fromLocal8Bit("�Ƿ���������˳�?"),
		QMessageBox::Yes | QMessageBox::No);
	
	if (button == QMessageBox::No) 
	{
		event->ignore();  //�����˳��źţ������������
	}
	else if (button == QMessageBox::Yes)
	{
		if (client_.get() != nullptr)
		{
			client_->close();
			ioservice_.stop();
		}
		event->accept();  //�����˳��źţ������˳�
	}
}

void Client::soltConnectServer()
{
// 	if (client_.get() != nullptr)
// 	{
// 		return;
// 	}
	tcp::resolver resolver(ioservice_);  //<----װ����һ��������
	tcp::resolver::query query(ui.lineEdit->text().toStdString().c_str(), 
		                       ui.lineEdit_2->text().toStdString().c_str());  //���IP��DNS��
	tcp::resolver::iterator iterator = resolver.resolve(query);

	client_ = boost::make_shared<chat_client>(ioservice_, iterator);

	work_ = boost::make_shared<io_service::work>(ioservice_);  //<-------û�й���io_serviceҲ����ֹͣ����

	//QT5�ķ���
	QObject::connect(client_.get(), SIGNAL(received_packet()), this, SLOT(handle_packet())); //<---�����������еĺ���Ϊһ����Ϣ��Ӧ

	boost::thread t(boost::bind(&boost::asio::io_service::run, &ioservice_));

	t.detach();
}

void Client::soltSendMessage()
{
	if (client_.get() != nullptr)
	{
		//QByteArray buf = ui.lineEdit_3->text().toLocal8Bit();
		QByteArray buf = ui.textEdit_2->toPlainText().toLocal8Bit();

		msg_.set_body_length(buf.size());  //���ð���
		memcpy(msg_.body(), buf.data(), buf.size());

		qDebug() << QString::fromLocal8Bit("Debug:��������:") << QString::fromLocal8Bit(buf.data())
			<< QString::fromLocal8Bit("���ͳ���:") << msg_.body_length();
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
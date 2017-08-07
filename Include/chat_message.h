#pragma once

class chat_message
{
public:
	enum
	{
		header_length_ = 4,    //包头
		                       //名字
		max_body_length_ = 512,//
	};

private:
	std::size_t body_length_;
	char data_[header_length_ + max_body_length_];

public:
	chat_message()
		:body_length_(0)
	{

	}

	const char* data() const
	{
		return data_;
	}

	char* data()
	{
		return data_;
	}

	const char* body() const
	{
		return data_ + header_length_;
	}

	char* body()
	{
		return data_ + header_length_;
	}

	std::size_t length()  //有效长度
	{
		return header_length_ + body_length_;
	}

	std::size_t header_length() const
	{
		return header_length_;
	}

	std::size_t body_length() const
	{
		return body_length_;
	}

	void set_body_length(size_t new_length)
	{
		body_length_ = new_length;
		if (body_length_ > max_body_length_)
		{
			body_length_ = max_body_length_;
		}
	}

	bool decode_header()  //把字符串转换成数字
	{
		char header[header_length_ + 1] = "";
		strncat(header, data_, header_length_);  //从封包里提取4字节

		body_length_ = atoi(header);
		if (body_length_ > max_body_length_)
		{
			body_length_ = 0;  //非法长度
			return false;  //滚犊子
		}
		return true;
	}

	bool encode_header()  //把数字转换成字串
	{
		char header[header_length_ + 1] = "";
		sprintf(header, "%4d", body_length_);  //真正的数据长度
		memcpy(data_, header, header_length_);
		return true;
	}
};
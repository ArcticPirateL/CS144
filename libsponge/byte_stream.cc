#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(const size_t capacity):_capacity(capacity){}

size_t ByteStream::write(const string &data) {
    if(_input_end_signal == true)                                         //输入停止，返回0
        return 0;
    size_t length = min(data.size(), _capacity - _bytes_buffer.size());   //确定写入长度
    _write_size += length;                                                //更新成员变量
    for(size_t i = 0; i < length; i++) {
        _bytes_buffer.push_back(data[i]);                                 //按字节加入队列
    }
    return length;                                                        //返回写入长度
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length = min(len, _bytes_buffer.size());                               //确定读出长度
    return string().assign(_bytes_buffer.begin(), _bytes_buffer.begin() + length);//返回读出的字节流
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
   size_t length = min(len, _bytes_buffer.size());          //确定读出长度
    _read_size += length;                                    //更新成员变量
    for(size_t i = 0; i < length; i++) {
        _bytes_buffer.pop_front();                           //按字节pop出数据
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
string ByteStream::read(const size_t len) {
    string result = this->peek_output(len);                  //result存放读出的数据
    this->pop_output(len);                                   //pop掉读出的数据
    return result;
}

void ByteStream::end_input() { _input_end_signal = true; }

bool ByteStream::input_ended() const { return _input_end_signal; }

size_t ByteStream::buffer_size() const { return _bytes_buffer.size(); }

bool ByteStream::buffer_empty() const { return _bytes_buffer.empty(); }

bool ByteStream::eof() const { return _input_end_signal && _bytes_buffer.empty(); }

size_t ByteStream::bytes_written() const { return _write_size; }

size_t ByteStream::bytes_read() const { return _read_size; }

size_t ByteStream::remaining_capacity() const { return _capacity - _bytes_buffer.size(); }


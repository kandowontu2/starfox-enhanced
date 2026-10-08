#pragma once
#include <ostream>
#include <string_view>
#include <cstddef>
#include <array>
#include <streambuf>

namespace starfox::vr {
// A failed/full diagnostic destination must never terminate gameplay or grow
// without bound. Each accepted line is flushed before the next native frame.
class BoundedDiagnosticLog {
public:
    explicit BoundedDiagnosticLog(std::ostream& destination,std::size_t limit=8U*1024U*1024U) noexcept
        :destination_(destination),limit_(limit) {}
    bool line(std::string_view text) noexcept {
        if(stopped_) return false;
        if(text.size()>=limit_-written_) {
            constexpr std::string_view marker="[diagnostic log size limit reached]";
            if(marker.size()<limit_-written_) write(marker);
            stopped_=true;return false;
        }
        return write(text);
    }
    [[nodiscard]] std::size_t written() const noexcept {return written_;}
    [[nodiscard]] bool stopped() const noexcept {return stopped_;}
private:
    bool write(std::string_view text) noexcept {
        try {
            destination_.write(text.data(),static_cast<std::streamsize>(text.size()));
            destination_.put('\n');destination_.flush();
            if(!destination_) {stopped_=true;return false;}
            written_+=text.size()+1;return true;
        } catch(...) {stopped_=true;return false;}
    }
    std::ostream& destination_;
    std::size_t limit_{},written_{};
    bool stopped_{};
};

// Preserve console output while retaining complete diagnostic lines. Console
// flushes (notably cerr's unitbuf) must not split every insertion into a line.
// Long/unfinished lines are bounded too; no per-frame heap allocation is needed.
class DiagnosticStreamBuffer final:public std::streambuf {
public:
    DiagnosticStreamBuffer(std::streambuf* console,BoundedDiagnosticLog& log) noexcept
        :console_(console),log_(log) {}
    void flush_pending() noexcept {
        if(size_) {log_.line(std::string_view(line_.data(),size_));size_=0;}
    }
protected:
    int_type overflow(int_type value) override {
        if(traits_type::eq_int_type(value,traits_type::eof())) return traits_type::not_eof(value);
        const auto ch=traits_type::to_char_type(value);
        if(!console_ || traits_type::eq_int_type(console_->sputc(ch),traits_type::eof()))
            return traits_type::eof();
        retain(ch);return value;
    }
    std::streamsize xsputn(const char* data,std::streamsize count) override {
        if(!console_ || count<=0) return 0;
        const auto accepted=console_->sputn(data,count);
        for(std::streamsize i=0;i<accepted;++i) retain(data[i]);
        return accepted;
    }
    int sync() override {return console_?console_->pubsync():-1;}
private:
    void retain(char ch) noexcept {
        if(ch=='\n') {log_.line(std::string_view(line_.data(),size_));size_=0;return;}
        line_[size_++]=ch;
        if(size_==line_.size()) flush_pending();
    }
    std::streambuf* console_{};
    BoundedDiagnosticLog& log_;
    std::array<char,4096> line_{};
    std::size_t size_{};
};

class ScopedDiagnosticTee {
public:
    ScopedDiagnosticTee(std::ostream& stream,BoundedDiagnosticLog& log)
        :stream_(stream),original_(stream.rdbuf()),buffer_(original_,log) {stream_.rdbuf(&buffer_);}
    ~ScopedDiagnosticTee() noexcept {
        buffer_.flush_pending();
        try {stream_.rdbuf(original_);} catch(...) {}
    }
    ScopedDiagnosticTee(const ScopedDiagnosticTee&)=delete;
    ScopedDiagnosticTee& operator=(const ScopedDiagnosticTee&)=delete;
private:
    std::ostream& stream_;
    std::streambuf* original_{};
    DiagnosticStreamBuffer buffer_;
};
}

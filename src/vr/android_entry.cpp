#include "starfox/vr/application.hpp"
#include "starfox/assets/embedded.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include "starfox/vr/diagnostic_log.hpp"
#include "starfox/assets/runtime_import.hpp"
#include <fstream>
#include <jni.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <android/log.h>
#include <iostream>
#include <streambuf>

namespace {
// Android discards ordinary process stderr. Preserve the shared application's
// diagnostics in app-scoped logcat instead of losing rendering failure details.
class AndroidErrors final : public std::streambuf {
    std::string line_;
    std::ostream& stream_;
    int priority_;
    std::streambuf* previous_;
    starfox::vr::BoundedDiagnosticLog* file_{};
    void flush_line() {
        if (!line_.empty()) {
            __android_log_write(priority_, "StarFoxVR", line_.c_str());
            if(file_) file_->line(line_);
            line_.clear();
        }
    }
    int_type overflow(int_type value) override {
        if (!traits_type::eq_int_type(value, traits_type::eof())) {
            const char c = traits_type::to_char_type(value);
            if (c == '\n') flush_line();
            else {line_ += c;if (line_.size() >= 1024) flush_line();}
        }
        return traits_type::not_eof(value);
    }
    int sync() override {flush_line();return 0;}
public:
    explicit AndroidErrors(std::ostream& stream=std::cerr,int priority=ANDROID_LOG_ERROR,
        starfox::vr::BoundedDiagnosticLog* file=nullptr)
        :stream_(stream),priority_(priority),previous_(stream.rdbuf(this)),file_(file) {}
    ~AndroidErrors() override {stream_.rdbuf(previous_);flush_line();}
};
class GlobalRef {
    JNIEnv* env_;
    jobject value_;
public:
    GlobalRef(JNIEnv* env, jobject value):env_(env),value_(value?env->NewGlobalRef(value):nullptr) {
        if(!value_) throw std::runtime_error("Missing Quest host reference");
    }
    ~GlobalRef(){env_->DeleteGlobalRef(value_);}
    GlobalRef(const GlobalRef&)=delete;
    jobject get() const {return value_;}
};
std::string path(JNIEnv* env,jstring value) {
    if(!value) throw std::runtime_error("Missing Quest asset path");
    const char* chars=env->GetStringUTFChars(value,nullptr);
    if(!chars) throw std::runtime_error("Cannot read Quest asset path");
    std::string result;
    try {result=chars;} catch(...) {env->ReleaseStringUTFChars(value,chars);throw;}
    env->ReleaseStringUTFChars(value,chars);
    if(result.empty()) throw std::runtime_error("Empty Quest asset path");
    return result;
}
}

// Invoked on a Java-owned worker thread, never on the Activity's UI thread.
extern "C" JNIEXPORT void JNICALL
Java_com_starfox_enhanced_quest_QuestBridge_validateBundle(JNIEnv* env,jclass,jstring file) {
    try {
        std::ifstream input(path(env,file),std::ios::binary|std::ios::ate);
        if(!input) throw std::runtime_error("Cannot open imported asset bundle");
        const auto size=input.tellg();
        if(size<=0 || size>64*1024*1024) throw std::runtime_error("Invalid asset bundle size");
        std::vector<std::uint8_t> bytes(static_cast<size_t>(size));input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))
            throw std::runtime_error("Incomplete asset bundle");
        (void)starfox::assets::decode_runtime_bundle(bytes,
            starfox::assets::runtime_companion_manifest(starfox::assets::embedded_asset));
    } catch(const std::exception& error) {
        if(!env->ExceptionCheck()) {
            const auto type=env->FindClass("java/lang/IllegalStateException");
            if(type) {env->ThrowNew(type,error.what());env->DeleteLocalRef(type);}
        }
    }
}
// All JNI access uses that thread's JNIEnv; no native handle outlives this call.
extern "C" JNIEXPORT void JNICALL
Java_com_starfox_enhanced_quest_QuestBridge_prepareInput(JNIEnv* env,jclass,jstring source,jstring destination) {
    try {
        const auto source_path = path(env, source);
        const auto destination_path = path(env, destination);
        if (source_path == destination_path) throw std::runtime_error("Import needs a separate output file");
        std::ifstream input(source_path, std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("Cannot open selected input");
        const auto size = input.tellg();
        if (size <= 0 || size > 64 * 1024 * 1024) throw std::runtime_error("Invalid input size");
        std::vector<std::uint8_t> bytes(static_cast<size_t>(size));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size())))
            throw std::runtime_error("Incomplete input file");
        const auto prepared = starfox::assets::prepare_runtime_input(bytes, starfox::assets::embedded_asset);
        std::ofstream output(destination_path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(prepared.bundle.data()), std::streamsize(prepared.bundle.size()));
        output.close();
        if (!output) throw std::runtime_error("Cannot write prepared assets; check free storage");
    } catch (const std::exception& error) {
        if (!env->ExceptionCheck()) {
            const auto type = env->FindClass("java/lang/IllegalStateException");
            if (type) {env->ThrowNew(type, error.what()); env->DeleteLocalRef(type);}
        }
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_starfox_enhanced_quest_QuestBridge_run(JNIEnv* env,jclass,jobject activity,
        jobject context,jstring rom,jstring symbols,jobject stop) {
    std::ofstream diagnostic_file;
    starfox::vr::BoundedDiagnosticLog file_log(diagnostic_file);
    AndroidErrors diagnostics(std::cerr,ANDROID_LOG_ERROR,&file_log);
    AndroidErrors progress(std::cout,ANDROID_LOG_INFO,&file_log);
    try {
        const auto rom_path=path(env,rom);
        const auto directory=std::filesystem::path(rom_path).parent_path();
        // Only the two app-owned diagnostic files are replaced. Keep the last
        // session when a tester relaunches after a native crash/process kill.
        std::error_code log_error;
        const auto current_log=directory/"vr-session.log";
        if(std::filesystem::exists(current_log,log_error))
            std::filesystem::copy_file(current_log,directory/"vr-session.previous.log",
                std::filesystem::copy_options::overwrite_existing,log_error);
        if(!log_error) diagnostic_file.open(current_log,std::ios::out|std::ios::trunc|std::ios::binary);
        if(!diagnostic_file.is_open())
            std::cerr<<"Persistent VR diagnostics unavailable; prior log retained and logcat remains active\n";
        std::cout<<"VR session start_unix_ms="
            <<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
            <<" diagnostic_cap=8388608 bytes\n";
        JavaVM* vm=nullptr;
        if(env->GetJavaVM(&vm)!=JNI_OK) throw std::runtime_error("Cannot obtain Java VM");
        GlobalRef activity_ref(env,activity),context_ref(env,context),stop_ref(env,stop);
        jclass stop_class=env->GetObjectClass(stop_ref.get());
        if(!stop_class) throw std::runtime_error("Missing cancellation class");
        const auto get=env->GetMethodID(stop_class,"get","()Z");
        env->DeleteLocalRef(stop_class);
        if(!get) throw std::runtime_error("Missing cancellation accessor");
        starfox::vr::AndroidXrContext android{vm,context_ref.get(),activity_ref.get()};
        starfox::vr::ApplicationHost host;
        host.android=&android;host.frame_limit=0;host.time_limit=std::chrono::seconds(0);
        host.stop_requested=[&] {
            const bool requested=env->CallBooleanMethod(stop_ref.get(),get)==JNI_TRUE;
            if(env->ExceptionCheck()) throw std::runtime_error("Quest cancellation callback failed");
            return requested;
        };
        std::vector<std::string> arguments{"starfox_quest",symbols?"--intro":"--bundle",rom_path};
        if(symbols) arguments.push_back(path(env,symbols));
        host.cartridge_save_path=std::filesystem::path(arguments[2]).parent_path()/"starfox-ex.srm";
        std::vector<char*> argv;
        for(auto& argument:arguments) argv.push_back(argument.data());
        const auto code=starfox::vr::run_application(static_cast<int>(argv.size()),argv.data(),host);
        std::cout<<"VR session returned code "<<code<<'\n';return code;
    } catch(const std::exception& error) {
        std::cerr<<"Quest native session failed: "<<error.what()<<'\n';
        if(!env->ExceptionCheck()) {
            const auto type=env->FindClass("java/lang/IllegalStateException");
            if(type) {env->ThrowNew(type,error.what());env->DeleteLocalRef(type);}
        }
    } catch(...) {
        std::cerr<<"Quest native session failed: unexpected exception\n";
        if(!env->ExceptionCheck()) {
            const auto type=env->FindClass("java/lang/IllegalStateException");
            if(type) {env->ThrowNew(type,"Unexpected Quest native failure");env->DeleteLocalRef(type);}
        }
    }
    return -1;
}
#include "starfox/assets/embedded.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include <fstream>

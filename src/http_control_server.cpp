extern "C"{
    #include "http_control_server.h"
};
#include "httplib.h"
#include <thread>
#include <iostream>
#include <sstream>
#include <vector>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>

// Live streams. The main thread publishes the newest frame (JPEG) and the sound, each streaming
// connection waits for them on its own server thread, without holding the callback lock.
#define HCS_STREAM_AUDIO_FRAMES (48000*2)   // Two seconds of 48 kHz stereo sound
#define HCS_STREAM_MAX_CLIENTS 4            // Per stream type, the server has a small thread pool
struct HCSStream{
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<uint8_t> frame;
    uint64_t frame_id = 0;
    std::vector<int16_t> audio = std::vector<int16_t>(HCS_STREAM_AUDIO_FRAMES*2);
    uint64_t audio_written = 0;              // Sample frames published so far
    std::atomic<int> video_clients{0}, audio_clients{0};
    std::atomic<bool> stopping{false};
};
static HCSStream hcs_stream;

static void hcs_start_mjpeg_stream(httplib::Response& res){
    hcs_stream.video_clients++;
    auto last_id = std::make_shared<uint64_t>(0);
    res.set_header("Cache-Control","no-cache, no-store");
    res.set_header("Access-Control-Allow-Origin","*");
    res.set_chunked_content_provider("multipart/x-mixed-replace; boundary=skyemuframe",
        [last_id](size_t, httplib::DataSink& sink){
            std::vector<uint8_t> frame;
            {
                std::unique_lock<std::mutex> lock(hcs_stream.mutex);
                // A paused game sends its frame again every second, which also notices closed connections
                hcs_stream.cv.wait_for(lock,std::chrono::seconds(1),[&]{return hcs_stream.stopping||hcs_stream.frame_id!=*last_id;});
                if(hcs_stream.stopping)return false;
                *last_id = hcs_stream.frame_id;
                frame = hcs_stream.frame;
            }
            if(frame.empty())return sink.is_writable();
            char header[128];
            int n = snprintf(header,sizeof(header),"--skyemuframe\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n",frame.size());
            return sink.write(header,n)&&sink.write((const char*)frame.data(),frame.size())&&sink.write("\r\n",2);
        },
        [](bool){hcs_stream.video_clients--;});
}

static void hcs_start_wav_stream(httplib::Response& res){
    hcs_stream.audio_clients++;
    auto position = std::make_shared<uint64_t>(UINT64_MAX);
    res.set_header("Cache-Control","no-cache, no-store");
    res.set_header("Access-Control-Allow-Origin","*");
    res.set_chunked_content_provider("audio/wav",
        [position](size_t, httplib::DataSink& sink){
            if(*position==UINT64_MAX){
                // Endless 16 bit stereo 48 kHz WAV, the sizes are left at their maximum
                uint8_t h[44] = {'R','I','F','F',0xff,0xff,0xff,0xff,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,
                                 0x80,0xbb,0,0,0x00,0xee,0x02,0,4,0,16,0,'d','a','t','a',0xff,0xff,0xff,0xff};
                {
                    std::lock_guard<std::mutex> lock(hcs_stream.mutex);
                    *position = hcs_stream.audio_written;
                }
                return sink.write((const char*)h,sizeof(h));
            }
            std::vector<int16_t> samples;
            {
                std::unique_lock<std::mutex> lock(hcs_stream.mutex);
                bool more = hcs_stream.cv.wait_for(lock,std::chrono::milliseconds(250),[&]{return hcs_stream.stopping||hcs_stream.audio_written>*position;});
                if(hcs_stream.stopping)return false;
                if(!more){
                    // Paused: silence keeps the players going and notices closed connections
                    lock.unlock();
                    samples.assign(48000/4*2,0);
                    return sink.write((const char*)samples.data(),samples.size()*sizeof(int16_t));
                }
                uint64_t written = hcs_stream.audio_written;
                // A connection that fell behind skips to recent sound
                if(written-*position>HCS_STREAM_AUDIO_FRAMES)*position = written-HCS_STREAM_AUDIO_FRAMES/4;
                samples.reserve((written-*position)*2);
                for(uint64_t i=*position;i<written;++i){
                    size_t at = (size_t)(i%HCS_STREAM_AUDIO_FRAMES)*2;
                    samples.push_back(hcs_stream.audio[at]);
                    samples.push_back(hcs_stream.audio[at+1]);
                }
                *position = written;
            }
            return sink.write((const char*)samples.data(),samples.size()*sizeof(int16_t));
        },
        [](bool){hcs_stream.audio_clients--;});
}

struct HCSServer{
    hcs_callback callback; 
    httplib::Server svr;
    std::recursive_mutex mutex;
    std::thread thread;
    int64_t port; 
    static void server_thread(HCSServer* server){
        server->svr.set_tcp_nodelay(true);
        server->svr.set_pre_routing_handler([server](const httplib::Request& req, httplib::Response& res) {
            std::vector<const char*> params;
            for(auto &v :req.params){
                params.push_back(v.first.c_str());
                params.push_back(v.second.c_str());
            }
            params.push_back(NULL);
            params.push_back(NULL);
            bool handled = false; 
            if(server->callback){
                uint64_t result_size = 0; 
                const char *mime_type = "";
                server->mutex.lock();
                uint8_t * result = server->callback(req.path.c_str(),&params[0],&result_size, &mime_type);
                server->mutex.unlock();
                // The callback allowed a live stream
                bool mjpeg = strcmp(mime_type,"x-skyemu/stream-mjpeg")==0, wav = strcmp(mime_type,"x-skyemu/stream-wav")==0;
                if(mjpeg||wav){
                    free(result);
                    if((mjpeg? hcs_stream.video_clients : hcs_stream.audio_clients)>=HCS_STREAM_MAX_CLIENTS){
                        res.status = 503;
                        res.set_content("Too many streams are open","text/plain");
                    }else if(mjpeg)hcs_start_mjpeg_stream(res);
                    else hcs_start_wav_stream(res);
                    return httplib::Server::HandlerResponse::Handled;
                }
                if(result&&result_size){
                    res.set_content((const char*)result,result_size,mime_type);
                    free(result);
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });
        std::cout<<"Starting HCS: http://localhost:"<<server->port<<std::endl;
        server->svr.listen("0.0.0.0",server->port);
        std::cout<<"Terminating HCS: http://localhost:"<<server->port<<std::endl;
    }
    HCSServer(int64_t port, hcs_callback call){
        callback = call; 
        this->port = port; 
        thread = std::thread(server_thread,this);
    }
    ~HCSServer(){
       svr.stop();
       thread.join();
    }
};
HCSServer * server = NULL;
extern "C"{
    void hcs_update(bool enable, int64_t port, hcs_callback callback){
        if(server)server->mutex.lock();
        if(server&&(!enable||port!=server->port)){
            server->mutex.unlock();
            // Streams wait for frames, wake them so the server can stop
            {
                std::lock_guard<std::mutex> lock(hcs_stream.mutex);
                hcs_stream.stopping = true;
            }
            hcs_stream.cv.notify_all();
            delete server;
            server = NULL;
            hcs_stream.stopping = false;
        }
        if(!server&&enable){
            server = new HCSServer(port, callback);
            server->mutex.lock();
        }
        if(server)server->mutex.unlock();
    }

    void hcs_suspend_callbacks(){
        if(server)server->mutex.lock();
    }
    void hcs_resume_callbacks(){
        if(server)server->mutex.unlock();
    }
    void hcs_join_server_thread(){
        if(server)server->thread.join();
    }
    void hcs_stream_publish_frame(const uint8_t* jpeg, size_t size){
        {
            std::lock_guard<std::mutex> lock(hcs_stream.mutex);
            hcs_stream.frame.assign(jpeg,jpeg+size);
            hcs_stream.frame_id++;
        }
        hcs_stream.cv.notify_all();
    }
    void hcs_stream_publish_audio(const int16_t* samples, size_t frames){
        if(!frames)return;
        {
            std::lock_guard<std::mutex> lock(hcs_stream.mutex);
            for(size_t i=0;i<frames;++i){
                size_t at = (size_t)((hcs_stream.audio_written+i)%HCS_STREAM_AUDIO_FRAMES)*2;
                hcs_stream.audio[at] = samples[i*2];
                hcs_stream.audio[at+1] = samples[i*2+1];
            }
            hcs_stream.audio_written+=frames;
        }
        hcs_stream.cv.notify_all();
    }
    int hcs_stream_video_clients(){return hcs_stream.video_clients;}
    int hcs_stream_audio_clients(){return hcs_stream.audio_clients;}
    // Address of this device on the local network, for links other devices can open
    const char* hcs_local_ip(){
        static char ip[64] = "localhost";
        static std::atomic<bool> done{false};
        if(done)return ip;
        done = true;
        int sock = (int)socket(AF_INET,SOCK_DGRAM,0);
        if(sock<0)return ip;
        sockaddr_in remote = {};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(53);
        inet_pton(AF_INET,"192.0.2.1",&remote.sin_addr);   // Never contacted, only picks the route
        sockaddr_in local = {};
        socklen_t len = sizeof(local);
        if(connect(sock,(sockaddr*)&remote,sizeof(remote))==0&&getsockname(sock,(sockaddr*)&local,&len)==0){
            char buffer[64];
            if(inet_ntop(AF_INET,&local.sin_addr,buffer,sizeof(buffer))&&strcmp(buffer,"0.0.0.0")!=0)snprintf(ip,sizeof(ip),"%s",buffer);
        }
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return ip;
    }
}
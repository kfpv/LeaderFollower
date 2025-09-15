#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <sstream>
#include <cstring>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <cctype>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Minimal Arduino compatibility shims
using String = std::string;
#define PROGMEM
#define FPSTR(x) (x)

// Pull in the embedded UI pieces
#include "../web_ui.h"

static std::string buildIndexHtml(){
    std::string html;
    html.reserve(8192);
    html += INDEX_HTML_PREFIX; // prefix includes opening tag & <script> start and SCHEMA_JSON marker
    html += FPSTR(ANIM_SCHEMA_JSON); // inject schema JSON
    html += INDEX_HTML_SUFFIX; // suffix closes the script string + rest
    return html;
}

static const char *STATE_JSON = R"({
  \"globalSpeed\":1.0,
  \"globalMin\":0.0,
  \"globalMax\":0.1,
  \"leader\":{\"animIndex\":1,\"speed\":3.0,\"phase\":0.0,\"width\":2,\"branchMode\":true,\"invert\":false},
  \"follower\":{\"animIndex\":1,\"speed\":3.0,\"phase\":0.0,\"width\":2,\"branchMode\":true,\"invert\":false}
})";

class SimpleHttpServer {
public:
    explicit SimpleHttpServer(int port):port_(port){}
    void start(){running_=true; thread_=std::thread([this]{run();});}
    void join(){ if(thread_.joinable()) thread_.join(); }
private:
    int port_; std::thread thread_; std::atomic<bool> running_{false};

    void run(){
        int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if(server_fd < 0){ perror("socket"); return; }
        int opt=1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_ANY); addr.sin_port=htons(port_);
        if(bind(server_fd, (sockaddr*)&addr, sizeof(addr))<0){ perror("bind"); close(server_fd); return; }
        if(listen(server_fd, 8)<0){ perror("listen"); close(server_fd); return; }
        std::cout << "Serving on http://localhost:"<<port_<<"\n";
        const std::string indexHtml = buildIndexHtml();
        while(running_){
            sockaddr_in caddr; socklen_t clen=sizeof(caddr);
            int cfd=accept(server_fd,(sockaddr*)&caddr,&clen);
            if(cfd<0){ if(errno==EINTR) continue; perror("accept"); break; }
            handleClient(cfd, indexHtml);
            close(cfd);
        }
        close(server_fd);
    }

    static void handleClient(int fd, const std::string &indexHtml){
        char buf[4096]; ssize_t n = recv(fd, buf, sizeof(buf)-1, 0); if(n<=0) return; buf[n]='\0';
        std::string req(buf);
        std::string method="GET"; std::string path="/";
        {
            std::istringstream iss(req); iss >> method >> path; // naive parse
        }
        if(path=="/" || path=="/index.html"){
            sendResponse(fd, 200, "text/html; charset=utf-8", indexHtml);
        } else if(path=="/api/state"){
            sendResponse(fd, 200, "application/json", STATE_JSON);
        } else if(path=="/api/cfg2"){
            // Just eat body and reply ok
            sendResponse(fd, 200, "application/json", R"({\"ok\":true})");
        } else {
            sendResponse(fd, 404, "text/plain", "Not found");
        }
    }

    static void sendResponse(int fd, int code, const std::string &ctype, const std::string &body){
        std::ostringstream oss;
        oss << "HTTP/1.1 "<<code<<" OK\r\n";
        oss << "Content-Type: "<<ctype<<"\r\n";
        oss << "Content-Length: "<< body.size() <<"\r\n";
        oss << "Connection: close\r\n\r\n";
        oss << body;
        auto s=oss.str(); send(fd, s.data(), s.size(), 0);
    }
};

int main(){
    SimpleHttpServer srv(8080);
    srv.start();
    // Run until Ctrl+C
    while(true) std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}

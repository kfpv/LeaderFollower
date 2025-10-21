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
  "globalSpeed": 1.0,
  "globalMin": 0.0,
  "globalMax": 0.1,
  "leader": {"animIndex": 1, "speed": 3.0, "phase": 0.0, "width": 2, "branchMode": true, "invert": false},
  "follower": {"animIndex": 1, "speed": 3.0, "phase": 0.0, "width": 2, "branchMode": true, "invert": false}
})";

// In-memory favorites store for test server
static std::vector<std::string> gFavorites;

static std::string extractNameFromJson(const std::string &js){
    auto pos = js.find("\"name\"");
    if (pos == std::string::npos) return "favorite";
    pos = js.find(':', pos);
    if (pos == std::string::npos) return "favorite";
    ++pos;
    while (pos < js.size() && js[pos] == ' ') ++pos;
    if (pos < js.size() && js[pos] == '"'){
        auto end = js.find('"', pos+1);
        if (end != std::string::npos && end > pos+1) return js.substr(pos+1, end - (pos+1));
    }
    // fallback until comma/brace
    size_t end = pos; while (end < js.size() && js[end] != ',' && js[end] != '}' && js[end] != '\n' && js[end] != '\r') ++end;
    return js.substr(pos, end-pos);
}

static std::string buildFavoritesJson(){
    std::ostringstream oss; oss << "{\"items\":[";
    for(size_t i=0;i<gFavorites.size();++i){
        if (i) oss << ",";
        const std::string &cfg = gFavorites[i];
        std::string name = extractNameFromJson(cfg);
        oss << "{\"id\":"<< i << ",\"name\":\"" << name << "\",\"cfg\":" << cfg << "}";
    }
    oss << "]}";
    return oss.str();
}

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

    static size_t findHeaderEnd(const std::string &req){
        auto pos = req.find("\r\n\r\n");
        if (pos == std::string::npos) return std::string::npos;
        return pos + 4;
    }

    static size_t parseContentLength(const std::string &req){
        size_t pos = 0; size_t clen = 0;
        while (true){
            auto p = req.find("\r\n", pos);
            if (p == std::string::npos) break;
            auto line = req.substr(pos, p - pos);
            if (line.empty()) break;
            const std::string key = "Content-Length:";
            if (line.size() >= key.size() && strncasecmp(line.c_str(), key.c_str(), key.size()) == 0){
                std::istringstream iss(line.substr(key.size()));
                iss >> clen; break;
            }
            pos = p + 2; // skip CRLF
        }
        return clen;
    }

    static void handleClient(int fd, const std::string &indexHtml){
        char buf[4096]; ssize_t n = recv(fd, buf, sizeof(buf)-1, 0); if(n<=0) return; buf[n]='\0';
        std::string req(buf);
        std::string method="GET"; std::string path="/";
        {
            std::istringstream iss(req); iss >> method >> path; // naive parse
        }

        // If POST, read full body by Content-Length
        std::string body;
        if (method == "POST"){
            size_t headerEnd = findHeaderEnd(req);
            size_t clen = parseContentLength(req);
            if (headerEnd != std::string::npos){
                body = req.substr(headerEnd);
                while (body.size() < clen){
                    ssize_t m = recv(fd, buf, sizeof(buf), 0);
                    if (m <= 0) break;
                    body.append(buf, buf + m);
                }
                if (body.size() > clen) body.resize(clen);
            }
        }

        if(path=="/" || path=="/index.html"){
            sendResponse(fd, 200, "text/html; charset=utf-8", indexHtml);
        } else if(path=="/api/state"){
            sendResponse(fd, 200, "application/json", STATE_JSON);
        } else if(path=="/api/cfg2"){
            // Just eat body and reply ok
            (void)body;
            sendResponse(fd, 200, "application/json", R"({\"ok\":true})");
        } else if(path=="/api/favorites" && method=="GET"){
            std::this_thread::sleep_for(std::chrono::seconds(1)); // simulate delay
            std::string json = buildFavoritesJson();
            sendResponse(fd, 200, "application/json", json);
        } else if(path=="/api/favorites/add" && method=="POST"){
            if (!body.empty()) gFavorites.push_back(body);
            std::ostringstream oss; oss << "{\"ok\":true,\"id\":" << (gFavorites.size()? gFavorites.size()-1 : 0) << "}";
            sendResponse(fd, 200, "application/json", oss.str());
        } else if(path=="/api/favorites/delete" && method=="POST"){
            // Parse {"id":n} from body
            int id = -1;
            if (!body.empty()){
                auto pos = body.find("\"id\"");
                if (pos != std::string::npos){
                    pos = body.find(':', pos);
                    if (pos != std::string::npos){
                        ++pos;
                        while (pos < body.size() && body[pos] == ' ') ++pos;
                        size_t end = pos;
                        while (end < body.size() && isdigit((unsigned char)body[end])) ++end;
                        if (end > pos) {
                            id = std::stoi(body.substr(pos, end - pos));
                        }
                    }
                }
            }
            if (id < 0 || id >= (int)gFavorites.size()){
                sendResponse(fd, 400, "application/json", "{\"ok\":false,\"error\":\"bad id\"}");
            } else {
                gFavorites.erase(gFavorites.begin() + id);
                sendResponse(fd, 200, "application/json", "{\"ok\":true}");
            }
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

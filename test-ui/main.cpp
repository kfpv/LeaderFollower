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
#include <cstdlib>
#include <ctime>

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

// --- Simulated dynamic animation state (mirrors firmware surface) ---
static int gLeaderAnimIndex = 1;
static int gFollowerAnimIndex = 1;
static std::unordered_map<int,float> gLeaderParams; // key: pid, value: number
static std::unordered_map<int,float> gFollowerParams;

static void initDefaultParams(){
    if (gLeaderParams.empty()){
        gLeaderParams[(int)AnimSchema::PID_GLOBAL_SPEED] = 1.0f;
        gLeaderParams[(int)AnimSchema::PID_GLOBAL_MIN] = 0.0f;
        gLeaderParams[(int)AnimSchema::PID_GLOBAL_MAX] = 1.0f;
    }
    if (gFollowerParams.empty()){
        gFollowerParams[(int)AnimSchema::PID_GLOBAL_SPEED] = 1.0f;
        gFollowerParams[(int)AnimSchema::PID_GLOBAL_MIN] = 0.0f;
        gFollowerParams[(int)AnimSchema::PID_GLOBAL_MAX] = 1.0f;
    }
}

// --- Auto mode simulated state ---
static bool gAutoOn = false;
static int gAutoIntervalMin = 1; // minutes
static bool gAutoRandom = false;
static std::vector<int> gAutoSel; // indices into gFavorites
static int gAutoIdx = -1; // index into gAutoSel
static std::chrono::steady_clock::time_point gAutoLastSwitch;

static void seedRandOnce(){ static bool s=false; if(!s){ std::srand((unsigned)std::time(nullptr)); s=true; } }

static void applyFavoriteSim(int favId){
    // Parse favorite JSON and apply to simulated dynamic state like firmware
    if (favId < 0 || favId >= (int)gFavorites.size()) return;
    initDefaultParams();
    const std::string &cfg = gFavorites[favId];
    auto findNum=[&](const std::string &s, size_t start, size_t end, const char *key, double defv){
        size_t k = s.find(key, start); if (k==std::string::npos || k>=end) return defv; k = s.find(':', k); if (k==std::string::npos || k>=end) return defv; k++; while (k<end && s[k]==' ') k++; size_t e=k; while (e<end && (std::isdigit((unsigned char)s[e])||s[e]=='-'||s[e]=='+'||s[e]=='.'||s[e]=='e'||s[e]=='E')) e++; try{ return std::stod(s.substr(k, e-k)); }catch(...){ return defv; } };
    auto parseBlock=[&](const char* blockName, int &animIndex, std::unordered_map<int,float> &pmap){
        size_t b = cfg.find(blockName); if (b==std::string::npos) return; size_t bl = cfg.find('{', b); if (bl==std::string::npos) return; size_t br = cfg.find('}', bl); if (br==std::string::npos) br = cfg.size();
        // animIndex
        double a = findNum(cfg, bl, br, "\"animIndex\"", animIndex);
        animIndex = (int)a;
        // scan id/value pairs in this block
        size_t pos = bl; int safety=0;
        while (pos < br && safety < 256){
            size_t idk = cfg.find("\"id\"", pos); if (idk==std::string::npos || idk>=br) break; size_t col = cfg.find(':', idk); if (col==std::string::npos) break; size_t is = col+1; while (is<br && cfg[is]==' ') is++; size_t ie=is; while (ie<br && std::isdigit((unsigned char)cfg[ie])) ie++; int pid = 0; try{ pid = std::stoi(cfg.substr(is, ie-is)); }catch(...){ pid = 0; }
            size_t vk = cfg.find("\"value\"", ie); if (vk==std::string::npos || vk>=br){ pos = ie; safety++; continue; }
            col = cfg.find(':', vk); if (col==std::string::npos) break; size_t vs = col+1; while (vs<br && cfg[vs]==' ') vs++; size_t ve=vs; while (ve<br && (std::isdigit((unsigned char)cfg[ve])||cfg[ve]=='-'||cfg[ve]=='+'||cfg[ve]=='.'||cfg[ve]=='e'||cfg[ve]=='E')) ve++; double v=0.0; try{ v = std::stod(cfg.substr(vs, ve-vs)); }catch(...){ v=0.0; }
            pmap[pid] = (float)v; pos = ve; safety++;
        }
    };
    parseBlock("\"leader\"", gLeaderAnimIndex, gLeaderParams);
    parseBlock("\"follower\"", gFollowerAnimIndex, gFollowerParams);
    // Optional globals
    size_t gpos = cfg.find("\"globals\""); if (gpos!=std::string::npos){ size_t gl = cfg.find('{', gpos); size_t gr = cfg.find('}', gl==std::string::npos?0:gl); if (gl!=std::string::npos && gr!=std::string::npos && gr>gl){ double gs = findNum(cfg, gl, gr, "\"globalSpeed\"", 1.0); double gmin = findNum(cfg, gl, gr, "\"globalMin\"", 0.0); double gmax = findNum(cfg, gl, gr, "\"globalMax\"", 1.0); gLeaderParams[(int)AnimSchema::PID_GLOBAL_SPEED]=(float)gs; gFollowerParams[(int)AnimSchema::PID_GLOBAL_SPEED]=(float)gs; gLeaderParams[(int)AnimSchema::PID_GLOBAL_MIN]=(float)gmin; gFollowerParams[(int)AnimSchema::PID_GLOBAL_MIN]=(float)gmin; gLeaderParams[(int)AnimSchema::PID_GLOBAL_MAX]=(float)gmax; gFollowerParams[(int)AnimSchema::PID_GLOBAL_MAX]=(float)gmax; } }
}

static void advanceAutoIfNeeded(){
    if (!gAutoOn) return;
    if (gAutoSel.empty()) { gAutoOn = false; return; }
    auto now = std::chrono::steady_clock::now();
    auto durSec = gAutoIntervalMin * 60;
    if (gAutoIdx < 0) {
        gAutoIdx = 0;
        gAutoLastSwitch = now;
        applyFavoriteSim(gAutoSel[gAutoIdx]);
        return;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - gAutoLastSwitch).count();
    if (elapsed >= durSec) {
        if (gAutoRandom) {
            seedRandOnce();
            int count = (int)gAutoSel.size();
            int newi = std::rand() % count;
            if (count > 1 && newi == gAutoIdx) newi = (newi + 1) % count;
            gAutoIdx = newi;
        } else {
            gAutoIdx = (gAutoIdx + 1) % (int)gAutoSel.size();
        }
        gAutoLastSwitch = now;
        applyFavoriteSim(gAutoSel[gAutoIdx]);
    }
}

static int remainingSeconds(){
    if (!gAutoOn || gAutoSel.empty() || gAutoIdx < 0) return 0;
    auto now = std::chrono::steady_clock::now();
    auto durSec = gAutoIntervalMin * 60;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - gAutoLastSwitch).count();
    int rem = durSec - (int)elapsed;
    if (rem < 0) rem = 0; return rem;
}

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

// Helpers to parse JSON-ish posts (very naive, matching firmware)
static int extractNum(const std::string &body, const char *key, int defv){
    auto idx = body.find(key); if (idx==std::string::npos) return defv;
    idx = body.find(':', idx); if (idx==std::string::npos) return defv; idx++;
    while (idx < body.size() && body[idx]==' ') idx++;
    size_t end=idx; while (end<body.size() && std::isdigit((unsigned char)body[end])) end++;
    if (end>idx) return std::stoi(body.substr(idx, end-idx));
    return defv;
}
static bool extractBool(const std::string &body, const char *key, bool defv){
    auto idx = body.find(key); if (idx==std::string::npos) return defv;
    idx = body.find(':', idx); if (idx==std::string::npos) return defv; idx++;
    while (idx < body.size() && body[idx]==' ') idx++;
    if (body.compare(idx, 4, "true") == 0) return true;
    if (body.compare(idx, 5, "false") == 0) return false;
    return defv;
}
static std::vector<int> extractSelections(const std::string &body){
    std::vector<int> out;
    auto sidx = body.find("\"selections\"");
    if (sidx==std::string::npos) return out;
    auto lb = body.find('[', sidx); auto rb = body.find(']', lb);
    if (lb==std::string::npos || rb==std::string::npos || rb<=lb) return out;
    size_t pos = lb+1;
    while (pos < rb){
        while (pos < rb && (body[pos]==' ' || body[pos]==',')) ++pos;
        size_t st = pos; while (pos < rb && std::isdigit((unsigned char)body[pos])) ++pos;
        if (pos > st) out.push_back(std::stoi(body.substr(st, pos-st)));
        while (pos < rb && body[pos] != ',') ++pos;
    }
    return out;
}

// Parse an array of objects with numeric id/value pairs under a key name
static std::vector<std::pair<int,float>> extractIdValueArray(const std::string &body, const char *key){
    std::vector<std::pair<int,float>> out;
    auto sidx = body.find(key);
    if (sidx==std::string::npos) return out;
    auto lb = body.find('[', sidx);
    auto rb = body.find(']', lb==std::string::npos?0:lb);
    if (lb==std::string::npos || rb==std::string::npos || rb<=lb) return out;
    size_t pos = lb+1; int safety=0;
    while (pos < rb && safety < 1024){
        auto idk = body.find("\"id\"", pos); if (idk==std::string::npos || idk>=rb) break;
        auto col = body.find(':', idk); if (col==std::string::npos || col>=rb) break; size_t is = col+1; while (is<rb && body[is]==' ') is++;
        size_t ie=is; while (ie<rb && std::isdigit((unsigned char)body[ie])) ie++;
        int pid = 0; try{ pid = std::stoi(body.substr(is, ie-is)); }catch(...){ pid=0; }
        auto vk = body.find("\"value\"", ie); if (vk==std::string::npos || vk>=rb){ pos = ie; safety++; continue; }
        col = body.find(':', vk); if (col==std::string::npos || col>=rb) break; size_t vs = col+1; while (vs<rb && body[vs]==' ') vs++;
        size_t ve=vs; while (ve<rb && (std::isdigit((unsigned char)body[ve])||body[ve]=='-'||body[ve]=='+'||body[ve]=='.'||body[ve]=='e'||body[ve]=='E')) ve++;
        float val = 0.0f; try{ val = std::stof(body.substr(vs, ve-vs)); }catch(...){ val=0.0f; }
        out.emplace_back(pid, val);
        pos = ve; safety++;
    }
    return out;
}

static void applyCfg2FromBody(const std::string &body){
    initDefaultParams();
    // role and animIndex
    int role = extractNum(body, "\"role\"", -1);
    int animIndex = extractNum(body, "\"animIndex\"", -1);
    auto params = extractIdValueArray(body, "\"params\"");
    auto globals = extractIdValueArray(body, "\"globals\"");
    if (role==0){ if (animIndex>=0) gLeaderAnimIndex = animIndex; for (auto &pr : params) gLeaderParams[pr.first]=pr.second; }
    else if (role==1){ if (animIndex>=0) gFollowerAnimIndex = animIndex; for (auto &pr : params) gFollowerParams[pr.first]=pr.second; }
    // Apply globals to both
    for (auto &pr : globals){ gLeaderParams[pr.first]=pr.second; gFollowerParams[pr.first]=pr.second; }
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
            // Build dynamic state including auto flags and current params
            advanceAutoIfNeeded();
            auto serializeParams=[&](const std::unordered_map<int,float> &m){
                std::ostringstream ps;
                ps << "[";
                bool first=true;
                for (const auto &kv : m){
                    if (!first) ps << ",";
                    first=false;
                    ps << "{\"id\":"<< kv.first <<",\"value\":"<< kv.second <<"}";
                }
                // Ensure globals exist with defaults if missing
                auto ensure=[&](int pid, float defv){
                    if (m.find(pid)==m.end()){
                        if (!first) ps << ",";
                        first=false;
                        ps << "{\"id\":"<< pid <<",\"value\":"<< defv <<"}";
                    }
                };
                ensure((int)AnimSchema::PID_GLOBAL_SPEED, 1.0f);
                ensure((int)AnimSchema::PID_GLOBAL_MIN,   0.0f);
                ensure((int)AnimSchema::PID_GLOBAL_MAX,   1.0f);
                ps << "]";
                return ps.str();
            };
            std::ostringstream s;
            s << "{";
            s << "\"autoOn\":" << (gAutoOn?"true":"false") << ",";
            s << "\"autoRemaining\":" << remainingSeconds() << ",";
            s << "\"leader\":{\"animIndex\":"<< gLeaderAnimIndex << ",\"params\":"<< serializeParams(gLeaderParams) << "},";
            s << "\"follower\":{\"animIndex\":"<< gFollowerAnimIndex << ",\"params\":"<< serializeParams(gFollowerParams) << "}";
            s << "}";
            sendResponse(fd, 200, "application/json", s.str());
        } else if(path=="/api/cfg2"){
            // Stop Auto when manual config is applied and apply body
            if (gAutoOn) {
                gAutoOn = false;
                std::cout << "[sim] Auto stopped due to cfg2" << std::endl;
            }
            applyCfg2FromBody(body);
            sendResponse(fd, 200, "application/json", "{\"ok\":true}");
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
                // Adjust selections: drop 'id' and shift higher indices down
                if (!gAutoSel.empty()){
                    std::vector<int> out; out.reserve(gAutoSel.size());
                    for (int v : gAutoSel){ if (v == id) continue; out.push_back(v > id ? v-1 : v); }
                    gAutoSel.swap(out);
                    if (gAutoSel.empty()) { gAutoOn=false; gAutoIdx=-1; }
                    else if (gAutoIdx >= (int)gAutoSel.size()) gAutoIdx = (int)gAutoSel.size()-1;
                }
                sendResponse(fd, 200, "application/json", "{\"ok\":true}");
            }
        } else if(path=="/api/auto/config" && method=="GET"){
            advanceAutoIfNeeded();
            // selections array reflects saved selection, not all favorites
            std::ostringstream sel; sel<<"["; for(size_t i=0;i<gAutoSel.size();++i){ if(i) sel<<","; sel<<gAutoSel[i]; } sel<<"]";
            int curId = -1; std::string curName="";
            if (gAutoOn && gAutoIdx >= 0 && gAutoIdx < (int)gAutoSel.size()){
                curId = gAutoSel[gAutoIdx];
                if (curId >= 0 && curId < (int)gFavorites.size()) curName = extractNameFromJson(gFavorites[curId]);
            }
            std::ostringstream j; j << "{\"on\":"<<(gAutoOn?"true":"false")
                                     << ",\"interval\":"<< gAutoIntervalMin
                                     << ",\"random\":"<<(gAutoRandom?"true":"false")
                                     << ",\"selections\":"<<sel.str()
                                     << ",\"current\":{\"name\":\""<<curName<<"\",\"id\":"<<curId<<",\"remaining\":"<< remainingSeconds() <<"}}";
            sendResponse(fd, 200, "application/json", j.str());
        } else if(path=="/api/auto/settings" && method=="POST"){
            int ivMin = extractNum(body, "\"interval\"", gAutoIntervalMin);
            if (ivMin < 1) ivMin = 1;
            bool rnd = extractBool(body, "\"random\"", gAutoRandom);
            auto sels = extractSelections(body);
            gAutoIntervalMin = ivMin;
            gAutoRandom = rnd;
            gAutoSel = std::move(sels);
            if (gAutoIdx >= (int)gAutoSel.size()) gAutoIdx = (int)gAutoSel.size()-1; // clamp
            sendResponse(fd, 200, "application/json", "{\"ok\":true}");
        } else if(path=="/api/auto/start" && method=="POST"){
            if (gAutoSel.empty()){
                sendResponse(fd, 400, "application/json", "{\"ok\":false,\"error\":\"no selections\"}");
            } else {
                gAutoOn = true;
                if (gAutoIdx < 0 || gAutoIdx >= (int)gAutoSel.size()) gAutoIdx = 0;
                gAutoLastSwitch = std::chrono::steady_clock::now();
                applyFavoriteSim(gAutoSel[gAutoIdx]);
                sendResponse(fd, 200, "application/json", "{\"ok\":true}");
            }
        } else if(path=="/api/auto/stop" && method=="POST"){
            gAutoOn = false;
            sendResponse(fd, 200, "application/json", "{\"ok\":true}");
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

#include "update_ui.h"
#include "pc_settings.h"
#include "achievement_ui.h"
#include "frame_pacer.h"
#include "settings_store.h"
#include "generated/default/tabletennis_init.h"
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <imgui.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>

namespace tabletennis::updates {
namespace {
std::mutex mutex;
std::condition_variable wake;
std::thread watcher;
std::thread checker;
bool stopping=false;
Status status;
std::string notified,installed;
std::function<void(std::string)> notify;
std::filesystem::path exec_folder;

std::string Read(const std::filesystem::path& path) {
  std::ifstream file(path,std::ios::binary);std::string result;
  for(char c;result.size()<8192 && file.get(c);) result+=c;
  return result;
}

#if !defined(_WIN32)
void OpenBrowser(const std::string& url) {
  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
      dup2(devnull, STDIN_FILENO);
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    execlp("xdg-open", "xdg-open", url.c_str(), (char*)nullptr);
    _exit(0);
  }
}

void DoBackgroundCheck(const std::filesystem::path& folder, const std::string& installed_ver) {
  auto updates_dir = folder.parent_path() / ".updates";
  auto status_path = updates_dir / "status.txt";
  std::error_code ec;
  std::filesystem::create_directories(updates_dir, ec);

  {
    std::lock_guard lock(mutex);
    status.state = "checking";
  }

  std::filesystem::path script;
  if (std::filesystem::is_regular_file(folder / "check_update.py", ec)) {
    script = folder / "check_update.py";
  } else if (std::filesystem::is_regular_file(folder.parent_path() / "project/tools/check_update.py", ec)) {
    script = folder.parent_path() / "project/tools/check_update.py";
  } else if (std::filesystem::is_regular_file(folder / "project/tools/check_update.py", ec)) {
    script = folder / "project/tools/check_update.py";
  }

  std::string cmd;
  if (!script.empty()) {
    cmd = "python3 \"" + script.string() + "\" --installed \"" + installed_ver + "\" --output \"" + status_path.string() + "\" 2>/dev/null";
  } else {
    cmd = "python3 -c \"import urllib.request, json, re, sys\n"
          "repo = 'Mabec-games/Table-Tennis-Rockstar-Linux'\n"
          "try:\n"
          "    req = urllib.request.Request(f'https://api.github.com/repos/{repo}/releases', headers={'User-Agent': 'TableTennis-Linux'})\n"
          "    with urllib.request.urlopen(req, timeout=6) as resp: data = json.loads(resp.read().decode('utf-8'))\n"
          "    latest, s_str, url = [0], '', f'https://github.com/{repo}/releases'\n"
          "    for item in data:\n"
          "        for t in [item.get('tag_name', '')] + [a.get('name', '') for a in item.get('assets', [])]:\n"
          "            m = re.search(r'(\\d+(?:\\.\\d+)+)', t)\n"
          "            if m:\n"
          "                v = [int(x) for x in m.group(1).split('.')]\n"
          "                if v > latest: latest = v; s_str = m.group(1); url = item.get('html_url', url)\n"
          "    inst_m = re.search(r'(\\d+(?:\\.\\d+)+)', '" + installed_ver + "')\n"
          "    installed_p = [int(x) for x in inst_m.group(1).split('.')] if inst_m else [0]\n"
          "    st = 'available' if latest > installed_p else 'up_to_date'\n"
          "    print(f'state={st}\\nversion={s_str or \"" + installed_ver + "\"}\\nurl={url}\\nmessage=')\n"
          "except Exception as e: print(f'state=failed\\nversion=" + installed_ver + "\\nmessage={e}')\" > \"" + status_path.string() + "\" 2>/dev/null";
  }

  int ret = std::system(cmd.c_str());
  (void)ret;

  auto fields = settings_store::Parse(Read(status_path));
  Status next{fields["state"], fields["version"], fields["url"], fields["message"]};
  {
    std::unique_lock lock(mutex);
    if (next.version == installed_ver && next.state == "available") next.state = "up_to_date";
    if (!next.state.empty()) status = std::move(next);
  }
}
#endif

bool Launch(const wchar_t* args) {
#if defined(_WIN32)
  auto executable=rex::filesystem::GetExecutableFolder()/"tabletennis_updater.exe";
  if(!std::filesystem::is_regular_file(executable)) return false;
  std::wstring command=L"\""+executable.wstring()+L"\" "+args;
  STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
  auto directory=executable.parent_path().wstring();
  if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,directory.c_str(),&startup,&process)) return false;
  CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
#else
  (void)args;
  return false;
#endif
}
}

void Start(std::function<void(std::string)> callback) {
  Stop();
  exec_folder=rex::filesystem::GetExecutableFolder();
  auto folder=exec_folder;
  auto fields=settings_store::Parse(Read(folder/"settings.ini"));
  installed=Read(folder/"install-version.txt");
  while(!installed.empty() && (installed.back()=='\r'||installed.back()=='\n')) installed.pop_back();
  if(installed.empty()) installed = "1.2";
  {
    std::lock_guard lock(mutex);stopping=false;notify=std::move(callback);notified.clear();status={};
  }
#if defined(_WIN32)
  if(fields["check_updates"]!="false") Launch(L"--check-only");
#else
  if(fields["check_updates"]!="false") {
    if (checker.joinable()) checker.join();
    checker = std::thread([folder, inst=installed]() {
      DoBackgroundCheck(folder, inst);
    });
  }
#endif
  watcher=std::thread([path=folder.parent_path()/".updates/status.txt"] {
    for(;;) {
      auto fields=settings_store::Parse(Read(path));
      Status next{fields["state"],fields["version"],fields["url"],fields["message"]};
      std::unique_lock lock(mutex);
      if(next.version==installed && next.state=="available") next.state="up_to_date";
      if(!next.state.empty()) status=std::move(next);
      if(wake.wait_for(lock,std::chrono::seconds(3),[]{return stopping;})) break;
    }
  });
}

void Stop() {
  {std::lock_guard lock(mutex);stopping=true;notify={};}
  wake.notify_all();
  if(watcher.joinable()) watcher.join();
  if(checker.joinable()) checker.join();
}

Status GetStatus() {std::lock_guard lock(mutex);return status;}

void CheckNow() {
#if !defined(_WIN32)
  if (checker.joinable()) checker.join();
  checker = std::thread([folder=exec_folder, inst=installed]() {
    DoBackgroundCheck(folder, inst.empty() ? "1.2" : inst);
  });
#else
  Launch(L"--check-only");
#endif
}

void Open() {
#if defined(_WIN32)
  if(!Launch(L"")) MessageBoxW(nullptr,L"Update support is missing. Install the current full release once to enable updates.",L"Table Tennis updates",MB_OK|MB_ICONINFORMATION);
#else
  Status cur = GetStatus();
  if (cur.state == "available") {
    std::string target_url = !cur.url.empty() ? cur.url : "https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/releases";
    OpenBrowser(target_url);
  } else {
    CheckNow();
    if (cur.state == "up_to_date") {
      OpenBrowser("https://github.com/Mabec-games/Table-Tennis-Rockstar-Linux/releases");
    }
  }
#endif
}

void NotifyInFrontend(PPCContext& ctx,uint8_t* base,uint32_t movie) {
  if(!movie || pc_settings::IsOpen() || achievement_ui::IsPanelOpen()) return;
  Status next=GetStatus();
  if(next.state!="available" || next.version.empty()) return;
  {
    std::lock_guard lock(mutex);if(next.version==notified) return;
  }
  auto call=ctx;call.r3.u64=movie;call.r4.u64=0x82060730;sub_823F9280(call,base);
  if(!call.r3.u32 || REX_LOAD_U32(call.r3.u32)!=3) return;
  std::function<void(std::string)> callback;
  {std::lock_guard lock(mutex);if(next.version==notified || !notify) return;notified=next.version;callback=notify;}
  callback(next.version);
}

UpdateToast::UpdateToast(rex::ui::ImGuiDrawer* drawer,std::string version)
  :ImGuiDialog(drawer),version_(std::move(version)) {pacing::SetUiActive(true);}
UpdateToast::~UpdateToast() {pacing::SetUiActive(false);}

void UpdateToast::OnDraw(ImGuiIO& io) {
  if(started_<0) started_=ImGui::GetTime();
  if(ImGui::GetTime()-started_>7) {Close();return;}
  const float s=io.DisplaySize.y/720.f,x=io.DisplaySize.x-610*s,y=io.DisplaySize.y-132*s;
  auto* draw=ImGui::GetForegroundDrawList();
  draw->AddRectFilled({x,y},{io.DisplaySize.x-22*s,y+104*s},IM_COL32(20,36,50,245),8*s);
  std::string title="UPDATE "+version_+" AVAILABLE";
  draw->AddText(achievement_ui::HeadlineFont(26*s),26*s,{x+20*s,y+16*s},IM_COL32(255,209,0,255),title.c_str());
  draw->AddText(achievement_ui::BodyFont(),18*s,{x+20*s,y+59*s},IM_COL32(235,237,240,255),"OPTIONS > PC SETTINGS > UPDATES");
}
}

// ============================================================
//  Main.cpp —— 界面层（Win32 API 图形界面）
//  实验五 综合应用程序：学生成绩管理系统
//
//  界面上的每个控件都有编号（ID），用户操作控件时 Windows 会
//  给窗口过程 WndProc 发送 WM_COMMAND / WM_NOTIFY 消息，
//  程序根据编号调用相应的处理函数，处理函数再调用 Model.h
//  中的类完成真正的数据操作。
// ============================================================
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <commctrl.h>

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "Model.h"

#pragma comment(lib, "comctl32.lib")

// 使用 6.0 版通用控件（界面更现代），没有这一行按钮会是 Win95 风格
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ------------------------------------------------------------
// 一、控件的编号
// ------------------------------------------------------------
enum ControlId {
    IDC_EDIT_ID = 1001,
    IDC_EDIT_NAME,
    IDC_COMBO_KIND,
    IDC_EDIT_MATH,
    IDC_EDIT_ENGLISH,
    IDC_EDIT_PROG,
    IDC_EDIT_RESEARCH,
    IDC_BTN_ADD,
    IDC_BTN_MODIFY,
    IDC_BTN_DELETE,
    IDC_BTN_CLEAR,
    IDC_LIST,
    IDC_COMBO_SORT,
    IDC_BTN_SORT,
    IDC_BTN_STAT,
    IDC_BTN_SAVE,
    IDC_BTN_IMPORT,
    IDC_STATIC_STATUS
};

// ------------------------------------------------------------
// 二、全局变量
// ------------------------------------------------------------
static HINSTANCE g_hInst = nullptr;
static HFONT g_hFont = nullptr;
static double g_scale = 1.0;            // 屏幕缩放比例（192 DPI 时为 2）
static Repository<Student> g_repo;      // 全部学生数据
static std::wstring g_filePath;         // students.txt 的完整路径
static bool g_refreshing = false;       // 正在刷新列表时忽略选中事件

static HWND g_hId = nullptr;
static HWND g_hName = nullptr;
static HWND g_hKind = nullptr;
static HWND g_hMath = nullptr;
static HWND g_hEnglish = nullptr;
static HWND g_hProg = nullptr;
static HWND g_hResearch = nullptr;
static HWND g_hList = nullptr;
static HWND g_hSort = nullptr;
static HWND g_hStatus = nullptr;

// ------------------------------------------------------------
// 三、字符串与数字的小工具
// ------------------------------------------------------------
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring Trim(const std::wstring& s) {
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return L"";
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::wstring Num(double v, int prec = 1) {
    std::wostringstream os;
    os << std::fixed << std::setprecision(prec) << v;
    return os.str();
}

static std::wstring GetWndText(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::vector<wchar_t> buf(len + 1, L'\0');
    GetWindowTextW(h, buf.data(), len + 1);
    return std::wstring(buf.data());
}

static void SetStatus(const std::wstring& text) {
    SetWindowTextW(g_hStatus, text.c_str());
}

static void ShowError(HWND hwnd, const std::string& msg) {
    MessageBoxW(hwnd, Utf8ToWide(msg).c_str(), L"提示",
                MB_OK | MB_ICONWARNING);
}

// ------------------------------------------------------------
// 四、读取编辑框中的成绩
// ------------------------------------------------------------
static double ReadScore(HWND hEdit, const std::wstring& field) {
    std::wstring text = Trim(GetWndText(hEdit));
    if (text.empty())
        throw ScoreException(WideToUtf8(field + L"不能为空"));

    size_t pos = 0;
    double v = 0;
    try {
        v = std::stod(text, &pos);
    } catch (...) {
        throw ScoreException(WideToUtf8(field + L"必须是数字"));
    }
    if (pos != text.size() || v < 0 || v > 100)
        throw ScoreException(WideToUtf8(field + L"必须在 0~100 之间"));
    return v;
}

static int ComboSel(HWND hCombo) {
    return (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
}

// ------------------------------------------------------------
// 五、列表的显示与刷新
// ------------------------------------------------------------
static void InitListColumns() {
    struct Column {
        const wchar_t* title;
        int width;
        int align;
    };
    static const Column cols[] = {
        {L"学号", 120, LVCFMT_LEFT},     {L"姓名", 110, LVCFMT_LEFT},
        {L"类别", 100, LVCFMT_LEFT},     {L"数学", 90, LVCFMT_RIGHT},
        {L"英语", 90, LVCFMT_RIGHT},     {L"程序设计", 110, LVCFMT_RIGHT},
        {L"科研", 90, LVCFMT_RIGHT},     {L"总分", 100, LVCFMT_RIGHT},
        {L"平均分", 110, LVCFMT_RIGHT},
    };
    for (int i = 0; i < 9; ++i) {
        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.fmt = cols[i].align;
        col.cx = (int)(cols[i].width * g_scale + 0.5);
        col.pszText = const_cast<wchar_t*>(cols[i].title);
        ListView_InsertColumn(g_hList, i, &col);
    }
    ListView_SetExtendedListViewStyle(
        g_hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
}

static void AddRow(int index, Student* s) {
    std::wstring cell[9];
    cell[0] = Utf8ToWide(s->id());
    cell[1] = Utf8ToWide(s->name());
    cell[2] = Utf8ToWide(s->type());
    cell[3] = Num(s->math(), 1);
    cell[4] = Num(s->english(), 1);
    cell[5] = Num(s->programming(), 1);
    cell[6] = L"-";
    cell[7] = Num(s->total(), 1);
    cell[8] = Num(s->average(), 2);

    Graduate* g = dynamic_cast<Graduate*>(s);
    if (g != nullptr) cell[6] = Num(g->research(), 1);

    LVITEMW item = {};
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.iSubItem = 0;
    item.pszText = const_cast<wchar_t*>(cell[0].c_str());
    ListView_InsertItem(g_hList, &item);

    for (int c = 1; c < 9; ++c) {
        ListView_SetItemText(g_hList, index, c,
                             const_cast<wchar_t*>(cell[c].c_str()));
    }
}

static void RefreshList() {
    g_refreshing = true;
    SendMessageW(g_hList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_hList);

    const std::vector<Student*>& items = g_repo.items();
    for (size_t i = 0; i < items.size(); ++i) AddRow((int)i, items[i]);

    SendMessageW(g_hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hList, nullptr, TRUE);
    g_refreshing = false;
}

// ------------------------------------------------------------
// 六、编辑框与列表之间的数据传递
// ------------------------------------------------------------
static void FillFields(Student* s) {
    SetWindowTextW(g_hId, Utf8ToWide(s->id()).c_str());
    SetWindowTextW(g_hName, Utf8ToWide(s->name()).c_str());

    Graduate* g = dynamic_cast<Graduate*>(s);
    SendMessageW(g_hKind, CB_SETCURSEL, g ? 1 : 0, 0);
    SetWindowTextW(g_hMath, Num(s->math(), 1).c_str());
    SetWindowTextW(g_hEnglish, Num(s->english(), 1).c_str());
    SetWindowTextW(g_hProg, Num(s->programming(), 1).c_str());
    SetWindowTextW(g_hResearch, g ? Num(g->research(), 1).c_str() : L"");
    EnableWindow(g_hResearch, g != nullptr);
}

static void ClearFields() {
    SetWindowTextW(g_hId, L"");
    SetWindowTextW(g_hName, L"");
    SendMessageW(g_hKind, CB_SETCURSEL, 0, 0);
    SetWindowTextW(g_hMath, L"");
    SetWindowTextW(g_hEnglish, L"");
    SetWindowTextW(g_hProg, L"");
    SetWindowTextW(g_hResearch, L"");
    EnableWindow(g_hResearch, FALSE);
    SetFocus(g_hId);
}

// ------------------------------------------------------------
// 七、各按钮的处理函数
// ------------------------------------------------------------
static void OnAdd(HWND hwnd) {
    std::string id = WideToUtf8(Trim(GetWndText(g_hId)));
    std::string name = WideToUtf8(Trim(GetWndText(g_hName)));
    if (id.empty()) throw ScoreException("学号不能为空");
    if (name.empty()) throw ScoreException("姓名不能为空");

    double m = ReadScore(g_hMath, L"高等数学成绩");
    double e = ReadScore(g_hEnglish, L"大学英语成绩");
    double p = ReadScore(g_hProg, L"程序设计成绩");

    Student* s = nullptr;
    if (ComboSel(g_hKind) == 1) {
        double r = ReadScore(g_hResearch, L"科研分");
        s = new Graduate(id, name, m, e, p, r);
    } else {
        s = new Student(id, name, m, e, p);
    }

    try {
        g_repo.add(s);  // 学号重复时抛异常
    } catch (...) {
        delete s;       // 添加失败，释放刚创建的对象
        throw;
    }

    RefreshList();
    ClearFields();
    SetStatus(L"添加成功，当前共 " + std::to_wstring(g_repo.size()) + L" 条记录");
}

static void OnModify(HWND hwnd) {
    std::string id = WideToUtf8(Trim(GetWndText(g_hId)));
    if (id.empty()) throw ScoreException("请先填写或选中要修改的学号");

    Student* s = g_repo.find(id);
    if (s == nullptr)
        throw ScoreException("未找到学号为 " + id + " 的学生");

    std::string name = WideToUtf8(Trim(GetWndText(g_hName)));
    if (name.empty()) throw ScoreException("姓名不能为空");

    double m = ReadScore(g_hMath, L"高等数学成绩");
    double e = ReadScore(g_hEnglish, L"大学英语成绩");
    double p = ReadScore(g_hProg, L"程序设计成绩");

    s->setName(name);
    s->setScores(m, e, p);
    if (dynamic_cast<Graduate*>(s) != nullptr) {
        double r = ReadScore(g_hResearch, L"科研分");
        static_cast<Graduate*>(s)->setResearch(r);
    }

    RefreshList();
    SetStatus(L"修改成功，当前共 " + std::to_wstring(g_repo.size()) + L" 条记录");
}

static void OnDelete(HWND hwnd) {
    std::string id = WideToUtf8(Trim(GetWndText(g_hId)));
    if (id.empty()) throw ScoreException("请先填写或选中要删除的学号");
    if (g_repo.find(id) == nullptr)
        throw ScoreException("未找到学号为 " + id + " 的学生");

    std::wstring msg = L"确定要删除学号为 " + Utf8ToWide(id) + L" 的记录吗？";
    if (MessageBoxW(hwnd, msg.c_str(), L"确认删除",
                    MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    g_repo.remove(id);
    RefreshList();
    ClearFields();
    SetStatus(L"删除成功，当前共 " + std::to_wstring(g_repo.size()) + L" 条记录");
}

static void OnSort(HWND hwnd) {
    if (g_repo.empty()) throw ScoreException("暂无记录，无法排序");

    std::vector<Student*>& v = g_repo.items();
    switch (ComboSel(g_hSort)) {
    case 0:
        std::sort(v.begin(), v.end(), [](Student* a, Student* b) {
            return a->average() > b->average();
        });
        break;
    case 1:
        std::sort(v.begin(), v.end(), [](Student* a, Student* b) {
            return a->total() > b->total();
        });
        break;
    case 2:
        std::sort(v.begin(), v.end(), [](Student* a, Student* b) {
            return a->id() < b->id();
        });
        break;
    default:
        std::sort(v.begin(), v.end(), [](Student* a, Student* b) {
            return a->name() < b->name();
        });
        break;
    }
    RefreshList();
    SetStatus(L"排序完成");
}

static void OnStatistics(HWND hwnd) {
    if (g_repo.empty()) throw ScoreException("暂无记录，无法统计");

    Statistics<Student> st = analyze(g_repo.items());
    int dist[5] = {0, 0, 0, 0, 0};
    for (Student* s : g_repo.items()) {
        double a = s->average();
        if (a >= 90) ++dist[0];
        else if (a >= 80) ++dist[1];
        else if (a >= 70) ++dist[2];
        else if (a >= 60) ++dist[3];
        else ++dist[4];
    }

    std::wostringstream os;
    os << L"记录总数：" << st.count << L" 人\n";
    os << L"班级平均分：" << Num(st.average, 2) << L"\n";
    os << L"最高分：" << Num(st.highest, 2) << L"（"
       << Utf8ToWide(st.highestName) << L"）\n";
    os << L"最低分：" << Num(st.lowest, 2) << L"（"
       << Utf8ToWide(st.lowestName) << L"）\n";
    os << L"及格人数：" << st.passed << L" 人，及格率："
       << Num(100.0 * st.passed / st.count, 1) << L"%\n\n";
    os << L"分数段分布：\n";
    os << L"    90~100：" << dist[0] << L" 人\n";
    os << L"    80~89 ：" << dist[1] << L" 人\n";
    os << L"    70~79 ：" << dist[2] << L" 人\n";
    os << L"    60~69 ：" << dist[3] << L" 人\n";
    os << L"    60 以下：" << dist[4] << L" 人";

    MessageBoxW(hwnd, os.str().c_str(), L"成绩统计",
                MB_OK | MB_ICONINFORMATION);
}

// 保存：把当前全部记录写入文本文件 students.txt
static void OnSave(HWND hwnd) {
    SaveToFile(g_repo.items(), g_filePath);
    SetStatus(L"已保存 " + std::to_wstring(g_repo.size()) +
              L" 条记录到 students.txt");
}

// 读取：从 students.txt 重新读取全部记录
static void OnLoad(HWND hwnd) {
    LoadFromFile(g_repo, g_filePath);
    RefreshList();
    ClearFields();
    SetStatus(L"已从 students.txt 读取 " +
              std::to_wstring(g_repo.size()) + L" 条记录");
}

// ------------------------------------------------------------
// 八、创建控件
// ------------------------------------------------------------
static HWND MakeButton(HWND parent, int id, const wchar_t* text,
                       int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(0, L"BUTTON", text,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                    BS_PUSHBUTTON,
                                x, y, w, h, parent, (HMENU)(INT_PTR)id,
                                g_hInst, nullptr);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hwnd;
}

static HWND MakeLabel(HWND parent, const wchar_t* text,
                      int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(0, L"STATIC", text,
                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                x, y, w, h, parent, nullptr, g_hInst, nullptr);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hwnd;
}

static HWND MakeEdit(HWND parent, int id, int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                    ES_AUTOHSCROLL,
                                x, y, w, h, parent, (HMENU)(INT_PTR)id,
                                g_hInst, nullptr);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hwnd;
}

static HWND MakeCombo(HWND parent, int id, int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                    CBS_DROPDOWNLIST | WS_VSCROLL,
                                x, y, w, h, parent, (HMENU)(INT_PTR)id,
                                g_hInst, nullptr);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return hwnd;
}

static BOOL CALLBACK ScaleChildProc(HWND child, LPARAM lp) {
    double s = *(double*)lp;
    HWND parent = GetParent(child);
    RECT rc;
    GetWindowRect(child, &rc);
    POINT tl = {rc.left, rc.top};
    POINT br = {rc.right, rc.bottom};
    ScreenToClient(parent, &tl);
    ScreenToClient(parent, &br);

    int x = (int)(tl.x * s + 0.5);
    int y = (int)(tl.y * s + 0.5);
    int w = (int)((br.x - tl.x) * s + 0.5);
    int h = (int)((br.y - tl.y) * s + 0.5);
    SetWindowPos(child, nullptr, x, y, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    return TRUE;
}

// 屏幕缩放到 150%、200% 时，把按 96 DPI 设计的控件坐标等比放大
static void ScaleChildWindows(HWND hwnd) {
    EnumChildWindows(hwnd, ScaleChildProc, (LPARAM)&g_scale);
}

static void CreateControls(HWND hwnd) {
    // 学生信息分组框（先创建，后创建的控件会盖在它上面）
    HWND group = CreateWindowExW(0, L"BUTTON", L"学生信息",
                                 WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                 16, 12, 1008, 106, hwnd, nullptr,
                                 g_hInst, nullptr);
    SendMessageW(group, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    MakeLabel(hwnd, L"学号", 32, 41, 36, 20);
    g_hId = MakeEdit(hwnd, IDC_EDIT_ID, 72, 38, 120, 26);

    MakeLabel(hwnd, L"姓名", 204, 41, 36, 20);
    g_hName = MakeEdit(hwnd, IDC_EDIT_NAME, 244, 38, 100, 26);

    MakeLabel(hwnd, L"类别", 356, 41, 36, 20);
    g_hKind = MakeCombo(hwnd, IDC_COMBO_KIND, 396, 38, 104, 200);
    SendMessageW(g_hKind, CB_ADDSTRING, 0, (LPARAM)L"本科生");
    SendMessageW(g_hKind, CB_ADDSTRING, 0, (LPARAM)L"研究生");
    SendMessageW(g_hKind, CB_SETCURSEL, 0, 0);

    MakeLabel(hwnd, L"数学", 512, 41, 36, 20);
    g_hMath = MakeEdit(hwnd, IDC_EDIT_MATH, 552, 38, 60, 26);

    MakeLabel(hwnd, L"英语", 624, 41, 36, 20);
    g_hEnglish = MakeEdit(hwnd, IDC_EDIT_ENGLISH, 664, 38, 60, 26);

    MakeLabel(hwnd, L"程序设计", 736, 41, 64, 20);
    g_hProg = MakeEdit(hwnd, IDC_EDIT_PROG, 804, 38, 60, 26);

    MakeLabel(hwnd, L"科研", 876, 41, 36, 20);
    g_hResearch = MakeEdit(hwnd, IDC_EDIT_RESEARCH, 916, 38, 60, 26);
    EnableWindow(g_hResearch, FALSE);  // 只有研究生才需要科研分

    MakeButton(hwnd, IDC_BTN_ADD, L"添加", 32, 74, 92, 30);
    MakeButton(hwnd, IDC_BTN_MODIFY, L"修改", 132, 74, 92, 30);
    MakeButton(hwnd, IDC_BTN_DELETE, L"删除", 232, 74, 92, 30);
    MakeButton(hwnd, IDC_BTN_CLEAR, L"清空", 332, 74, 92, 30);
    MakeLabel(hwnd,
              L"提示：单击列表中的一行可把该记录填入上方；"
              L"修改、删除按“学号”进行。",
              440, 79, 570, 20);

    // 成绩列表
    g_hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                  LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                              16, 132, 1008, 380, hwnd,
                              (HMENU)(INT_PTR)IDC_LIST, g_hInst, nullptr);
    SendMessageW(g_hList, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    InitListColumns();

    // 底部按钮
    MakeLabel(hwnd, L"排序方式", 16, 529, 64, 20);
    g_hSort = MakeCombo(hwnd, IDC_COMBO_SORT, 84, 524, 140, 200);
    SendMessageW(g_hSort, CB_ADDSTRING, 0, (LPARAM)L"按平均分从高到低");
    SendMessageW(g_hSort, CB_ADDSTRING, 0, (LPARAM)L"按总分从高到低");
    SendMessageW(g_hSort, CB_ADDSTRING, 0, (LPARAM)L"按学号从小到大");
    SendMessageW(g_hSort, CB_ADDSTRING, 0, (LPARAM)L"按姓名排序");
    SendMessageW(g_hSort, CB_SETCURSEL, 0, 0);

    MakeButton(hwnd, IDC_BTN_SORT, L"排序", 232, 524, 80, 30);
    MakeButton(hwnd, IDC_BTN_STAT, L"统计", 322, 524, 80, 30);
    MakeButton(hwnd, IDC_BTN_SAVE, L"保存", 412, 524, 80, 30);
    MakeButton(hwnd, IDC_BTN_IMPORT, L"读取", 502, 524, 80, 30);

    // 状态栏（用静态文本模拟，显示操作结果）
    g_hStatus = CreateWindowExW(0, L"STATIC", L"就绪",
                                WS_CHILD | WS_VISIBLE |
                                    SS_LEFTNOWORDWRAP | SS_SUNKEN,
                                16, 566, 1008, 26, hwnd,
                                (HMENU)(INT_PTR)IDC_STATIC_STATUS,
                                g_hInst, nullptr);
    SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

// ------------------------------------------------------------
// 九、窗口过程：接收并分发消息
// ------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hwnd);
        if (g_scale != 1.0) ScaleChildWindows(hwnd);
        SetFocus(g_hId);

        // 启动时如果 students.txt 存在，自动读入
        {
            std::error_code ec;
            if (std::filesystem::exists(g_filePath, ec)) {
                try {
                    LoadFromFile(g_repo, g_filePath);
                    RefreshList();
                    SetStatus(L"已自动读取 " +
                              std::to_wstring(g_repo.size()) + L" 条记录");
                } catch (const std::exception&) {
                    SetStatus(L"数据文件读取失败，可重新录入");
                }
            }
        }
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        try {
            switch (id) {
            case IDC_BTN_ADD:
                if (code == BN_CLICKED) OnAdd(hwnd);
                break;
            case IDC_BTN_MODIFY:
                if (code == BN_CLICKED) OnModify(hwnd);
                break;
            case IDC_BTN_DELETE:
                if (code == BN_CLICKED) OnDelete(hwnd);
                break;
            case IDC_BTN_CLEAR:
                if (code == BN_CLICKED) {
                    ClearFields();
                    SetStatus(L"已清空输入框");
                }
                break;
            case IDC_BTN_SORT:
                if (code == BN_CLICKED) OnSort(hwnd);
                break;
            case IDC_BTN_STAT:
                if (code == BN_CLICKED) OnStatistics(hwnd);
                break;
            case IDC_BTN_SAVE:
                if (code == BN_CLICKED) OnSave(hwnd);
                break;
            case IDC_BTN_IMPORT:
                if (code == BN_CLICKED) OnLoad(hwnd);
                break;
            case IDC_COMBO_KIND:
                if (code == CBN_SELCHANGE)
                    EnableWindow(g_hResearch, ComboSel(g_hKind) == 1);
                break;
            }
        } catch (const ScoreException& e) {
            ShowError(hwnd, e.what());
        } catch (const std::exception& e) {
            ShowError(hwnd, e.what());
        }
        return 0;
    }

    case WM_NOTIFY: {
        LPNMHDR nh = (LPNMHDR)lParam;
        if (nh->idFrom == IDC_LIST && nh->code == LVN_ITEMCHANGED) {
            LPNMLISTVIEW nv = (LPNMLISTVIEW)lParam;
            bool newlySelected = (nv->uNewState & LVIS_SELECTED) &&
                                 !(nv->uOldState & LVIS_SELECTED);
            if (!g_refreshing && newlySelected) {
                wchar_t id[64] = {0};
                ListView_GetItemText(g_hList, nv->iItem, 0, id, 64);
                Student* s = g_repo.find(WideToUtf8(id));
                if (s != nullptr) FillFields(s);
            }
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ------------------------------------------------------------
// 十、程序入口
// ------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    g_hInst = hInst;
    SetProcessDPIAware();

    // 屏幕缩放：96 DPI 为 100%，192 DPI 为 200%
    HDC screenDC = GetDC(nullptr);
    int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
    ReleaseDC(nullptr, screenDC);
    g_scale = dpi / 96.0;

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    // 程序所在目录 + students.txt
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::filesystem::path exeFile(exePath);
    g_filePath = (exeFile.parent_path() / L"students.txt").wstring();

    // 使用系统的界面字体（通常是微软雅黑），字号按 DPI 换算成 9 磅
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -MulDiv(9, dpi, 72);
    g_hFont = CreateFontIndirectW(&ncm.lfMessageFont);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"StudentManagerWnd";
    RegisterClassW(&wc);

    // 固定大小的窗口（客户区按 96 DPI 设计，再按缩放比例放大）
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc = {0, 0, (LONG)(1040 * g_scale), (LONG)(612 * g_scale)};
    AdjustWindowRect(&rc, style, FALSE);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowW(L"StudentManagerWnd",
                              L"学生成绩管理系统 —— 实验五 综合应用程序",
                              style, x, y, w, h, nullptr, nullptr, hInst, nullptr);
    if (hwnd == nullptr) return 0;

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_hFont);
    return 0;
}

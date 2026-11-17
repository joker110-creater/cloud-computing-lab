// ============================================================
//  Model.cpp —— 数据模型层的实现（文件读写）
// ============================================================
#include "Model.h"

#include <filesystem>
#include <istream>

// 保存全部记录：每行一条，字段之间用 '|' 分隔
void SaveToFile(const std::vector<Student*>& items, const std::wstring& path) {
    std::filesystem::path file(path);
    std::ofstream fout(file);
    if (!fout)
        throw ScoreException("无法打开文件用于写入：students.txt");

    for (const Student* p : items) {
        p->save(fout);  // 虚函数：研究生会多写一个科研分
        fout << "\n";
    }
}

// 由一行文本创建对象：首字符 U 为本科生，G 为研究生
static Student* CreateFromLine(const std::string& line) {
    std::istringstream iss(line);
    std::string tag, id, name;
    double m = 0, e = 0, p = 0, r = 0;

    std::getline(iss, tag, '|');
    std::getline(iss, id, '|');
    std::getline(iss, name, '|');
    iss >> m;
    iss.ignore(1, '|');
    iss >> e;
    iss.ignore(1, '|');
    iss >> p;

    if (tag == "U") return new Student(id, name, m, e, p);
    if (tag == "G") {
        iss.ignore(1, '|');
        iss >> r;
        return new Graduate(id, name, m, e, p, r);
    }
    throw ScoreException("数据文件格式错误：" + line);
}

void LoadFromFile(Repository<Student>& repo, const std::wstring& path) {
    std::filesystem::path file(path);
    std::ifstream fin(file);
    if (!fin) throw ScoreException("无法打开数据文件 students.txt");

    Repository<Student> temp;
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        temp.add(CreateFromLine(line));  // 重复学号会抛异常
    }

    repo.clear();
    for (Student* p : temp.items()) repo.add(p);
    temp.items().clear();  // 所有权已转移，避免重复释放
}

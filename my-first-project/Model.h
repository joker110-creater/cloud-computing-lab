// ============================================================
//  Model.h —— 数据模型层（与界面无关）
//  实验五 综合应用程序：学生成绩管理系统
//
//  本文件只负责“数据”和“业务规则”：
//    1. Person / Student / Graduate 三个类，体现继承与多态
//    2. Repository<T> 类模板，负责对象集合的管理
//    3. 运算符重载、函数模板、统计、文件读写
//  界面代码在 Main.cpp 中，界面只调用本文件提供的接口。
// ============================================================
#pragma once

#include <algorithm>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ------------------------------------------------------------
// 一、异常类：成绩非法、学号重复、文件打不开时抛出
// ------------------------------------------------------------
class ScoreException : public std::runtime_error {
public:
    explicit ScoreException(const std::string& msg) : std::runtime_error(msg) {}
};

// ------------------------------------------------------------
// 二、抽象基类 Person：描述“人”的公共属性和接口
// ------------------------------------------------------------
class Person {
public:
    Person(const std::string& id, const std::string& name)
        : id_(id), name_(name) {}
    virtual ~Person() {}

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    virtual double total() const = 0;      // 总分
    virtual double average() const = 0;    // 平均分（研究生为综合成绩）
    virtual std::string type() const = 0;  // 类别名称
    virtual char tag() const = 0;          // 存文件时的类型标记
    virtual void save(std::ostream& os) const = 0;

protected:
    std::string id_;    // 学号
    std::string name_;  // 姓名
};

// ------------------------------------------------------------
// 三、Student：本科生，保存三门课程成绩
// ------------------------------------------------------------
class Student : public Person {
public:
    Student(const std::string& id = "", const std::string& name = "",
            double math = 0, double english = 0, double programming = 0)
        : Person(id, name), math_(math), english_(english),
          programming_(programming) {}

    double math() const { return math_; }
    double english() const { return english_; }
    double programming() const { return programming_; }

    // 成绩合法性检查，非法时抛出异常
    void setScores(double math, double english, double programming) {
        if (math < 0 || math > 100 || english < 0 || english > 100 ||
            programming < 0 || programming > 100)
            throw ScoreException("成绩必须在 0~100 之间");
        math_ = math;
        english_ = english;
        programming_ = programming;
    }

    double total() const override { return math_ + english_ + programming_; }
    double average() const override { return total() / 3.0; }
    std::string type() const override { return "本科生"; }
    char tag() const override { return 'U'; }

    void save(std::ostream& os) const override {
        os << tag() << "|" << id_ << "|" << name_ << "|"
           << math_ << "|" << english_ << "|" << programming_;
    }

    // -------- 运算符重载 --------
    bool operator<(const Student& rhs) const { return average() < rhs.average(); }
    bool operator==(const std::string& id) const { return id_ == id; }

    Student operator+(double bonus) const {
        Student tmp(*this);
        tmp += bonus;
        return tmp;
    }

    Student& operator+=(double bonus) {
        auto clamp = [](double v) {
            return v > 100 ? 100.0 : (v < 0 ? 0.0 : v);
        };
        math_ = clamp(math_ + bonus);
        english_ = clamp(english_ + bonus);
        programming_ = clamp(programming_ + bonus);
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const Student& s) {
        os << s.id() << " " << s.name() << " " << s.type() << " "
           << s.math() << " " << s.english() << " " << s.programming()
           << " " << s.total() << " " << s.average();
        return os;
    }

protected:
    double math_;
    double english_;
    double programming_;
};

// ------------------------------------------------------------
// 四、Graduate：研究生，增加科研分，重定义综合成绩
//    综合成绩 = 课程平均分 × 70% + 科研分 × 30%
// ------------------------------------------------------------
class Graduate : public Student {
public:
    Graduate(const std::string& id = "", const std::string& name = "",
             double math = 0, double english = 0, double programming = 0,
             double research = 0)
        : Student(id, name, math, english, programming), research_(research) {}

    double research() const { return research_; }

    void setResearch(double research) {
        if (research < 0 || research > 100)
            throw ScoreException("科研分必须在 0~100 之间");
        research_ = research;
    }

    double average() const override {
        return Student::average() * 0.7 + research_ * 0.3;
    }

    std::string type() const override { return "研究生"; }
    char tag() const override { return 'G'; }

    void save(std::ostream& os) const override {
        Student::save(os);  // 基类用虚函数 tag() 写类型标记，研究生写 G
        os << "|" << research_;
    }

private:
    double research_;
};

// ------------------------------------------------------------
// 五、类模板 Repository<T>：对象集合的增加、查找、删除
//    实际存放 Student*，可以指向 Student 或 Graduate 对象，
//    通过基类指针调用虚函数即可实现多态。
// ------------------------------------------------------------
template <typename T>
class Repository {
public:
    Repository() = default;
    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;
    ~Repository() { clear(); }

    void add(T* item) {
        if (find(item->id()) != nullptr)
            throw ScoreException("学号 " + item->id() + " 已存在，不能重复录入");
        items_.push_back(item);
    }

    T* find(const std::string& id) const {
        for (T* p : items_)
            if (p->id() == id) return p;
        return nullptr;
    }

    bool remove(const std::string& id) {
        for (auto it = items_.begin(); it != items_.end(); ++it) {
            if ((*it)->id() == id) {
                delete *it;
                items_.erase(it);
                return true;
            }
        }
        return false;
    }

    void clear() {
        for (T* p : items_) delete p;
        items_.clear();
    }

    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }
    std::vector<T*>& items() { return items_; }
    const std::vector<T*>& items() const { return items_; }

private:
    std::vector<T*> items_;
};

// ------------------------------------------------------------
// 六、函数模板：统计分析
// ------------------------------------------------------------
template <typename T>
struct Statistics {
    size_t count = 0;
    double average = 0;
    double highest = 0;
    double lowest = 0;
    std::string highestName;
    std::string lowestName;
    size_t passed = 0;
};

template <typename T>
Statistics<T> analyze(const std::vector<T*>& items) {
    Statistics<T> st;
    st.count = items.size();
    if (items.empty()) return st;

    auto addAverage = [](double sum, const T* p) { return sum + p->average(); };
    st.average = std::accumulate(items.begin(), items.end(), 0.0, addAverage);
    st.average /= items.size();

    auto byAverage = [](const T* a, const T* b) {
        return a->average() < b->average();
    };
    auto hi = std::max_element(items.begin(), items.end(), byAverage);
    auto lo = std::min_element(items.begin(), items.end(), byAverage);
    st.highest = (*hi)->average();
    st.highestName = (*hi)->name();
    st.lowest = (*lo)->average();
    st.lowestName = (*lo)->name();
    st.passed = std::count_if(items.begin(), items.end(),
                              [](const T* p) { return p->average() >= 60.0; });
    return st;
}

// ------------------------------------------------------------
// 七、文件保存与读取
//    本科生：U|学号|姓名|数学|英语|程序设计
//    研究生：G|学号|姓名|数学|英语|程序设计|科研分
// ------------------------------------------------------------
void SaveToFile(const std::vector<Student*>& items, const std::wstring& path);
void LoadFromFile(Repository<Student>& repo, const std::wstring& path);

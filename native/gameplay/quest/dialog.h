#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 对话系统（阶段二）：线性台词序列 + 会话推进。
// 台词库内置于 native，确定性可测试；UI 仅负责渲染与推进请求。
struct DialogLine {
  std::string speaker;
  std::string text;
};

struct DialogDef {
  int32_t id = 0;
  std::vector<DialogLine> lines;
};

// 台词库：首条主线相关对话。
class DialogLibrary {
 public:
  static const DialogLibrary& defaults();

  // 按 id 查找对话；不存在返回 nullptr。
  const DialogDef* find(int32_t dialogId) const;
  const std::vector<DialogDef>& dialogs() const { return dialogs_; }

 private:
  std::vector<DialogDef> dialogs_;
};

// 单次对话会话：记录当前台词索引，advance 推进并在结束时返回 false。
class DialogSession {
 public:
  void start(const DialogDef* def);
  bool active() const;
  // 推进到下一句；若已是最后一句则结束会话并返回 false。
  bool advance();
  const DialogLine* current() const;
  int32_t index() const { return index_; }
  int32_t lineCount() const;

 private:
  const DialogDef* def_ = nullptr;
  int32_t index_ = 0;
};
